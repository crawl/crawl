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
static void mons_lose_attack_energy(monsters *attacker, int wpn_speed,
                                    int which_attack);

/*
 **************************************************
 *                                                *
 *             BEGIN PUBLIC FUNCTIONS             *
 *                                                *
 **************************************************
*/

bool test_melee_hit(int to_hit, int ev)
{
    int   roll = -1;
    int margin = AUTOMATIC_HIT;

    ev *= 2;

    if (to_hit >= AUTOMATIC_HIT)
        return (true);
    else if (random2(1000) < 10 * MIN_HIT_MISS_PERCENTAGE)
        margin = (coinflip() ? 1 : -1) * AUTOMATIC_HIT;
    else
    {
        roll = random2( to_hit + 1 );
        margin = (roll - random2avg(ev, 2));
    }

#if DEBUG_DIAGNOSTICS
    float miss;

    if (to_hit < ev)
        miss = 100.0 - static_cast<float>( MIN_HIT_MISS_PERCENTAGE ) / 2.0;
    else
    {
        miss = static_cast<float>( MIN_HIT_MISS_PERCENTAGE ) / 2.0
              + static_cast<float>( (100 - MIN_HIT_MISS_PERCENTAGE) * ev ) 
                  / static_cast<float>( to_hit );
    }

    mprf( MSGCH_DIAGNOSTICS,
          "to hit: %d; ev: %d; miss: %0.2f%%; roll: %d; result: %s%s (%d)",
              to_hit, ev, miss, roll, (margin >= 0) ? "hit" : "miss", 
              (roll == -1) ? "!!!" : "", margin );
#endif

    return (margin >= 0);
}

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

            defender->hurt(attacker, defender->hit_points);
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
                                    int damage_done, int damage_type = -1)
{
    if (defender->id() == MONS_HYDRA)
    {
        const int dam_type =
            damage_type != -1? damage_type : attacker->damage_type();
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
      cancel_attack(false),
      did_hit(false), perceived_attack(false),
      needs_message(false), attacker_visible(false), defender_visible(false),
      unarmed_ok(allow_unarmed),
      attack_number(which_attack),
      to_hit(0), base_damage(0), potential_damage(0), damage_done(0),
      special_damage(0), aux_damage(0), stab_attempt(false), stab_bonus(0),
      weapon(NULL), damage_brand(SPWPN_NORMAL),
      wpn_skill(SK_UNARMED_COMBAT), hands(HANDS_ONE),
      spwld(SPWLD_NONE), hand_half_bonus(false),
      art_props(0), attack_verb(), verb_degree(),
      no_damage_message(), special_damage_message(), unarmed_attack(),
      shield(NULL), defender_shield(NULL),
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
    if (defender)
        defender_shield = defender->shield();

    water_attack = is_water_attack(attacker, defender);
    attacker_visible = attacker->visible();
    defender_visible = defender && defender->visible();
    needs_message = attacker_visible || defender_visible;
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

void melee_attack::identify_mimic(monsters *mon)
{
    if (mon && mons_is_mimic(mon->type)
        && mons_near(mon) && player_monster_visible(mon))
    {
        mon->flags |= MF_KNOWN_MIMIC;
    }
}

bool melee_attack::attack()
{
    // If a mimic is attacking or defending, it is thereafter known.
    identify_mimic(atk);
    identify_mimic(def);

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

    if (cancel_attack)
        return (false);

    coord_def where = defender->pos();
    
    if (player_hits_monster())
    {
        did_hit = true;
        if (Options.tutorial_left)
            Options.tut_melee_counter++;        

        // This actually does more than calculate damage - it also sets up
        // messages, etc.
        player_calc_hit_damage();

        // always upset monster regardless of damage
        behaviour_event(def, ME_WHACK, MHITYOU);
        
        if (player_hurt_monster())
            player_exercise_combat_skills();
        
        if (player_check_monster_died())
            return (true);

        player_sustain_passive_damage();

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
    
    if (unarmed_ok && where == defender->pos() && player_aux_unarmed())
        return (true);

    if ((did_primary_hit || did_hit) && def->alive()
        && where == defender->pos())
    {
        print_wounds(def);
    }

    return (did_primary_hit || did_hit);
}

// Returns true to end the attack round.
bool melee_attack::player_aux_unarmed()
{
    damage_brand  = SPWPN_NORMAL;
    int uattack = UNAT_NO_ATTACK;
    bool simple_miss_message = false;

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
                simple_miss_message = true;
            }
            else if (you.has_usable_claws())
            {
                unarmed_attack = "claw";
                aux_damage += roll_dice(1, 3);
                simple_miss_message = true;
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
        if (to_hit >= def->ev || one_chance_in(30))
        {
            if (player_apply_aux_unarmed())
                return (true);
        }
        else
        {
            if (simple_miss_message)
                mprf("You miss %s.",
                     defender->name(DESC_NOCAP_THE).c_str());
            else
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
        hurt_monster(def, aux_damage);

    damage_done = aux_damage;
    
    if (damage_done > 0)
    {
        player_exercise_combat_skills();

        mprf("You %s %s%s%s",
             unarmed_attack.c_str(),
             defender->name(DESC_NOCAP_THE).c_str(),
             debug_damage_number().c_str(),
             attack_strength_punctuation().c_str());
                     
        if (damage_brand == SPWPN_VENOM && coinflip())
            poison_monster( def, KC_YOU );
                    
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
    if (special_damage < 6)
        return ".";
    else
        return "!";
}

std::string melee_attack::attack_strength_punctuation()
{
    if (attacker->atype() == ACT_PLAYER)
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
    else
    {
        return (damage_done < HIT_WEAK? "." : "!");
    }
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
    if ((to_hit + heavy_armour_penalty / 2) >= def->ev)
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
          to_hit, def->ev );
#endif

    return (to_hit >= def->ev
            || one_chance_in(20)
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
        && weapon->base_type == OBJ_WEAPONS
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
                int stun = random2( you.dex + 1 );
                
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
        if (def->ac > 0)
        {
            int ac = def->ac
                - random2( you.skills[SK_STABBING] / stab_bonus );
            
            if (ac > 0)
                damage -= random2(1 + ac);
        }
    }
    else
    {
        // apply AC normally
        if (def->ac > 0)
            damage -= random2(1 + def->ac);
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
        if (you.damage_type() == DVORP_CLAWING)
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

    // These two (staff damage and damage brand) are mutually exclusive!
    player_apply_staff_damage();

    // Returns true if the monster croaked.
    if (apply_damage_brand())
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
    
    if (needs_message && !special_damage_message.empty())
        mprf("%s", special_damage_message.c_str());


    hurt_monster(def, special_damage);

    if (def->hit_points < 1)
    {
        monster_die(def, KILL_YOU, 0);
        return (true);
    }

    return (false);
}

int melee_attack::resist_adjust_damage(int res, int rawdamage)
{
    if (res > 0)
    {
        if (defender->atype() == ACT_MONSTER)
            rawdamage = 0;
        else
            rawdamage /= 1 + res * res;
    }
    else if (res < 0)
        rawdamage *= 2;

    if (rawdamage < 0)
        rawdamage = 0;

    return (rawdamage);
}

void melee_attack::calc_elemental_brand_damage(
    int res,
    const char *verb)
{
    special_damage = resist_adjust_damage(res, random2(damage_done) / 2 + 1);

    if (special_damage > 0 && verb)
    {
        special_damage_message = make_stringf(
            "%s %s %s%s",
            attacker->name(DESC_CAP_THE).c_str(),
            attacker->conj_verb(verb).c_str(),
            defender->name(DESC_NOCAP_THE).c_str(),
            special_attack_punctuation().c_str());
    }
}

int melee_attack::fire_res_apply_cerebov_downgrade(int res)
{
    if (weapon && weapon->special == SPWPN_SWORD_OF_CEREBOV)
        --res;

    return (res);
}

void melee_attack::drain_defender()
{
    // What to do, they're different...
    if (defender->atype() == ACT_PLAYER)
        drain_player();
    else
        drain_monster();
}

void melee_attack::drain_player()
{
    drain_exp();
    special_damage =
        random2(damage_done) / (2 + defender->res_negative_energy()) + 1;

    // Noop for monsters, but what the heck.
    attacker->god_conduct(DID_NECROMANCY, 2);
}

void melee_attack::drain_monster()
{
    if (defender->res_negative_energy() > 0 || one_chance_in(3))
        return;

    special_damage_message =
        make_stringf(
            "%s %s %s!",
            attacker->name(DESC_CAP_THE).c_str(),
            attacker->conj_verb("drain").c_str(),
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
    attacker->god_conduct(DID_NECROMANCY, 2);
}

bool melee_attack::distortion_affects_defender()
{
    //jmf: blink frogs *like* distortion
    // I think could be amended to let blink frogs "grow" like
    // jellies do {dlb}
    if (defender->id() == MONS_BLINK_FROG)
    {
        if (one_chance_in(5))
        {
            emit_nodmg_hit_message();
            special_damage_message =
                make_stringf("%s %s in the translocular energy.",
                             defender->name(DESC_CAP_THE).c_str(),
                             defender->conj_verb("bask").c_str());
                
            defender->heal(1 + random2avg(7, 2), true); // heh heh
        }
        return (false);
    }
        
    if (one_chance_in(3))
    {
        special_damage_message =
            make_stringf(
                "Space bends around %s.",
                defender->name(DESC_NOCAP_THE).c_str());
        special_damage += 1 + random2avg(7, 2);
        return (false);
    }

    if (one_chance_in(3))
    {
        special_damage_message =
            make_stringf(
                "Space warps horribly around %s!",
                defender->name(DESC_NOCAP_THE).c_str());
            
        special_damage += 3 + random2avg(24, 2);
        return (false);
    }

    if (one_chance_in(3))
    {
        emit_nodmg_hit_message();
        defender->blink();
        return (false);
    }

    // Used to be coinflip() || coinflip() for players, just coinflip()
    // for monsters; this is a compromise. Note that it makes banishment
    // a touch more likely for players, and a shade less likely for
    // monsters.
    if (!one_chance_in(3))
    {
        emit_nodmg_hit_message();
        defender->teleport(coinflip(), one_chance_in(5));
        return (false);
    }

    if (you.level_type != LEVEL_ABYSS && coinflip())
    {
        emit_nodmg_hit_message();
        defender->banish( atk? atk->name(DESC_PLAIN, true)
                          : attacker->name(DESC_PLAIN) );
        return (true);
    }

    return (false);
}

bool melee_attack::apply_damage_brand()
{
    // Monster resistance to the brand.
    int res = 0;
    
    special_damage = 0;
    switch (damage_brand)
    {
    case SPWPN_FLAMING:
        res = fire_res_apply_cerebov_downgrade( defender->res_fire() );
        calc_elemental_brand_damage(res, "burn");
        defender->expose_to_element(BEAM_FIRE);
        break;

    case SPWPN_FREEZING:
        calc_elemental_brand_damage(defender->res_cold(), "freeze");
        defender->expose_to_element(BEAM_COLD);
        break;

    case SPWPN_HOLY_WRATH:
        switch (defender->holiness())
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
        {
            special_damage_message =
                make_stringf(
                    "%s %s%s",
                    defender->name(DESC_CAP_THE).c_str(),
                    defender->conj_verb("convulse").c_str(),
                    special_attack_punctuation().c_str());
        }
        break;

    case SPWPN_ELECTROCUTION:
        if (defender->levitates())
            break;
        else if (defender->res_elec() > 0)
            break;
        else if (one_chance_in(3))
        {
            special_damage_message =
                defender->atype() == ACT_PLAYER?
                   "You are electrocuted!"
                :  "There is a sudden explosion of sparks!";
            special_damage = 10 + random2(15);
        }
        break;

    case SPWPN_ORC_SLAYING:
        if (defender->mons_species() == MONS_ORC)
        {
            special_damage = 1 + random2(damage_done);
            special_damage_message =
                make_stringf(
                    "%s %s%s",
                    defender->name(DESC_CAP_THE).c_str(),
                    defender->conj_verb("convulse").c_str(),
                    special_attack_punctuation().c_str());
        }
        break;

    case SPWPN_VENOM:
    case SPWPN_STAFF_OF_OLGREB:
        if (!one_chance_in(4))
        {
            // Poison monster message needs to arrive after hit message.
            emit_nodmg_hit_message();

            // Weapons of venom do two levels of poisoning to the player,
            // but only one level to monsters.
            defender->poison( attacker, 2 );
        }
        break;

    case SPWPN_DRAINING:
        drain_defender();
        break;
        
        /* 9 = speed - done before */
    case SPWPN_VORPAL:
        special_damage = 1 + random2(damage_done) / 2;
        // note: leaving special_damage_message empty because there
        // isn't one.
        break;

    case SPWPN_VAMPIRICISM:
        if (defender->holiness() != MH_NATURAL ||
            defender->res_negative_energy() > 0 ||
            damage_done < 1 || attacker->stat_hp() == attacker->stat_maxhp() ||
            one_chance_in(5))
        {
            break;
        }
        
        // We only get here if we've done base damage, so no
        // worries on that score.

        if (attacker->atype() == ACT_PLAYER)
            mpr("You feel better.");
        else if (attacker_visible)
        {
            if (defender->atype() == ACT_PLAYER)
                mprf("%s draws strength from your injuries!",
                     attacker->name(DESC_CAP_THE).c_str());
            else
                mprf("%s is healed.",
                     attacker->name(DESC_CAP_THE).c_str());
        }

        {
            int hp_boost = 0;
            
            // thus is probably more valuable on larger weapons?
            if (weapon
                && is_fixed_artefact( *weapon ) 
                && weapon->special == SPWPN_VAMPIRES_TOOTH)
            {
                hp_boost = damage_done;
            }
            else
            {
                hp_boost = 1 + random2(damage_done);
            }

            attacker->heal(hp_boost);
        }

        if (attacker->hunger_level() != HS_ENGORGED)
            attacker->make_hungry(-random2avg(59, 2));
        
        attacker->god_conduct( DID_NECROMANCY, 2 );
        break;

    case SPWPN_DISRUPTION:
        if (defender->holiness() == MH_UNDEAD && !one_chance_in(3))
        {
            special_damage_message =
                defender->atype() == ACT_MONSTER?
                make_stringf("%s %s.",
                             defender->name(DESC_CAP_THE).c_str(),
                             defender->conj_verb("shudder").c_str())
                : ("You are blasted by holy energy!");
            
            special_damage += random2avg((1 + (damage_done * 3)), 3);
        }
        break;
        
    case SPWPN_PAIN:
        if (defender->res_negative_energy() <= 0
            && random2(8) <= attacker->skill(SK_NECROMANCY))
        {
            special_damage_message =
                make_stringf("%s %s in agony.",
                             defender->name(DESC_CAP_THE).c_str(),
                             defender->conj_verb("writhe").c_str());
            special_damage += random2( 1 + attacker->skill(SK_NECROMANCY) );
        }
        attacker->god_conduct(DID_NECROMANCY, 4);
        break;

    case SPWPN_DISTORTION:
        if (distortion_affects_defender())
            return (true);
        break;

    case SPWPN_CONFUSE:
    {
        // FIXME: Generalise.
        if (defender->atype() != ACT_MONSTER || attacker->atype() != ACT_PLAYER)
            break;
        
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
    {
        // cap chance at 30% -- let staff of Olgreb shine
        int temp_rand = damage_done + you.skills[SK_POISON_MAGIC];

        if (temp_rand > 30)
            temp_rand = 30;

        if (random2(100) < temp_rand)
        {
            // Poison monster message needs to arrive after hit message.
            emit_nodmg_hit_message();
            poison_monster(def, KC_YOU);
        }
        break;
    }

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
    damage_done =
        potential_damage > 0? one_chance_in(3) + random2(potential_damage) : 0;
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
        && def->has_ench(ENCH_BACKLIGHT))
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
    if (def->has_ench(ENCH_CONFUSION) 
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
        stab_attempt = (random2(130) <= you.skills[SK_STABBING] + you.dex);

    if (stab_attempt && you.religion == GOD_SHINING_ONE)
    {
        if (!yesno("Really attack this helpless creature?", false, 'n'))
        {
            stab_attempt  = false;
            cancel_attack = true;
        }
    }

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
    mons_perform_attack();
    if (perceived_attack && (def->foe == MHITNOT || one_chance_in(3))
        && atk->alive())
    {
        def->foe = monster_index(atk);
    }
    return (did_hit);
}

bool melee_attack::mons_self_destructs()
{
    if (atk->type == MONS_GIANT_SPORE || atk->type == MONS_BALL_LIGHTNING)
    {
        atk->hit_points = -1;
        return (true);
    }
    return (false);
}

bool melee_attack::mons_attack_warded_off()
{
    const int holy = defender->holy_aura();
    if (holy && mons_holiness(atk) == MH_UNDEAD
        && !check_mons_resist_magic( atk, holy ))
    {
        if (needs_message)
        {
            mprf("%s tries to attack %s, but is repelled by %s holy aura.",
                 atk->name(DESC_CAP_THE).c_str(),
                 defender->name(DESC_NOCAP_THE).c_str(),
                 defender->pronoun(PRONOUN_NOCAP_POSSESSIVE).c_str());
        }
        return (true);
    }

    // [dshaligram] Note: warding is no longer a simple 50% chance.
    const int warding = defender->warding();
    if (warding
        && mons_is_summoned(atk)
        && !check_mons_resist_magic( atk, warding ))
    {
        if (needs_message)
        {
            mprf("%s tries to attack %s, but flinches away.",
                 atk->name(DESC_CAP_THE).c_str(),
                 defender->name(DESC_NOCAP_THE).c_str());
        }
        return (true);
    }

    return (false);
}

int melee_attack::mons_attk_delay()
{
    return (weapon? property(*weapon, PWPN_SPEED) : 0);
}

bool melee_attack::attack_shield_blocked(bool verbose)
{
    if (!defender_shield)
        return (false);

    if (defender->incapacitated())
        return (false);

    const int con_block = random2(attacker->shield_bypass_ability(to_hit)
                                  + defender->shield_block_penalty());
    const int pro_block = defender->shield_bonus();
    
    if (pro_block >= con_block)
    {
        perceived_attack = true;

        if (needs_message && verbose)
            mprf("%s %s %s attack.",
                 defender->name(DESC_CAP_THE).c_str(),
                 defender->conj_verb("block").c_str(),
                 attacker->name(DESC_NOCAP_YOUR).c_str());

        defender->shield_block_succeeded();

        return (true);
    }

    return (false);
}

int melee_attack::mons_calc_damage(const mon_attack_def &attk)
{
    int damage = 0;
    int damage_max = 0;
    if (weapon
        && weapon->base_type == OBJ_WEAPONS
        && !is_range_weapon(*weapon))
    {
        damage_max = property( *weapon, PWPN_DAMAGE );
        damage += random2( damage_max );

        if (get_equip_race(*weapon) == ISFLAG_ORCISH
            && mons_species(atk->type) == MONS_ORC
            && coinflip())
        {
            damage++;
        }

        if (weapon->plus2 >= 0)
            damage += random2( weapon->plus2 );
        else
            damage -= random2( 1 - weapon->plus2 );

        damage -= 1 + random2(3);
    }

    damage_max += attk.damage;
    damage     += 1 + random2(attk.damage);

    // Berserk monsters get bonus damage.
    if (atk->has_ench(ENCH_BERSERK))
        damage = damage * 3 / 2;

    if (water_attack)
        damage *= 2;

    return (mons_apply_defender_ac(damage, damage_max));
}

int melee_attack::mons_apply_defender_ac(int damage, int damage_max)
{
    int ac = defender->armour_class();
    if (ac > 0)
    {
        int damage_reduction = random2(ac + 1);
        if (!defender->wearing_light_armour())
        {
            if (const item_def *arm = defender->slot_item(EQ_BODY_ARMOUR))
            {
                const int armac = property(*arm, PARM_AC);
                int perc = 2 * (defender->skill(SK_ARMOUR) + armac);
                if (perc > 50)
                    perc = 50;

                int min = 1 + (damage_max * perc) / 100;
                if (min > ac / 2)
                    min = ac / 2;

                if (damage_reduction < min)
                    damage_reduction = min;
            }
        }
        damage -= damage_reduction;
    }

    if (damage < 1)
        damage = 0;
    
    return damage;
}

std::string melee_attack::mons_attack_verb(const mon_attack_def &attk)
{
    static const char *attack_types[] =
    {
        "",
        "hit",         // including weapon attacks
        "bite",
        "sting",

        // spore
        "hit",
        
        "touch",
        "engulf",
        "claw",
        "tail slap",
        "butt"
    };

    return attack_types[ attk.type ];
}

std::string melee_attack::mons_weapon_desc()
{
    if (weapon && attacker->id() != MONS_DANCING_WEAPON)
        return make_stringf(" with %s", item_name(*weapon, DESC_NOCAP_A));

    return ("");
}

std::string melee_attack::mons_defender_name()
{
    if (attacker == defender)
        return attacker->pronoun(PRONOUN_REFLEXIVE);
    else
        return defender->name(DESC_NOCAP_THE);
}

void melee_attack::mons_announce_hit(const mon_attack_def &attk)
{
    if (water_attack && attacker_visible && attacker != defender)
        mprf("%s uses the watery terrain to its advantage.",
             attacker->name(DESC_CAP_THE).c_str());

    if (needs_message)
        mprf("%s %s %s%s%s%s",
             attacker->name(DESC_CAP_THE).c_str(),
             attacker->conj_verb( mons_attack_verb(attk) ).c_str(),
             mons_defender_name().c_str(),
             debug_damage_number().c_str(),
             mons_weapon_desc().c_str(),
             attack_strength_punctuation().c_str());
}

void melee_attack::mons_announce_dud_hit(const mon_attack_def &attk)
{
    if (needs_message)
        mprf("%s %s %s but doesn't do any damage.",
             attacker->name(DESC_CAP_THE).c_str(),
             attacker->conj_verb( mons_attack_verb(attk) ).c_str(),
             mons_defender_name().c_str());
}

void melee_attack::check_defender_train_dodging()
{
    // It's possible to train both dodging and armour under the new scheme.
    if (defender->wearing_light_armour(true)
        && attacker_visible
        && one_chance_in(3))
    {
        perceived_attack = true;
        defender->exercise(SK_DODGING, 1);
    }
}

void melee_attack::check_defender_train_armour()
{
    if (defender->wearing_light_armour())
        return;
    
    const item_def *arm = defender->slot_item(EQ_BODY_ARMOUR);
    if (arm && coinflip() && random2(1000) <= item_mass(*arm))
        defender->exercise(SK_ARMOUR, coinflip()? 2 : 1);
}

void melee_attack::mons_set_weapon(const mon_attack_def &attk)
{
    weapon = attk.type == AT_HIT? attacker->weapon(attack_number) : NULL;
    damage_brand = attacker->damage_brand(attack_number);
}

void melee_attack::mons_do_poison(const mon_attack_def &attk)
{
    if (defender->res_poison() > 0)
        return;
    
    if (attk.flavour == AF_POISON_NASTY
        || one_chance_in( 15 + 5 * (attk.flavour == AF_POISON) )
        || (damage_done > 1
            && one_chance_in( attk.flavour == AF_POISON? 4 : 3 )))
    {
        if (needs_message)
        {
            if (defender->atype() == ACT_PLAYER
                && (attk.type == AT_BITE || attk.type == AT_STING))
            {
                mprf("%s %s was poisonous!",
                     attacker->name(DESC_CAP_YOUR).c_str(),
                     mons_attack_verb(attk).c_str());
            }
            else
                mprf("%s poisons %s!",
                     attacker->name(DESC_CAP_THE).c_str(),
                     defender->name(DESC_NOCAP_THE).c_str());
        }

        int amount = 1;
        if (attk.flavour == AF_POISON_NASTY)
            amount++;
        else if (attk.flavour == AF_POISON_MEDIUM)
            amount += random2(3);
        else if (attk.flavour == AF_POISON_STRONG)
            amount += roll_dice(2, 5);

        defender->poison( attacker, amount );
    }
}

void melee_attack::wasp_paralyse_defender()
{
    // [dshaligram] Adopted 4.1.2's wasp mechanics, in slightly modified form.
    if (attacker->id() == MONS_RED_WASP || one_chance_in(3))
        defender->poison( attacker, coinflip()? 2 : 1 );

    int paralyse_roll = (damage_done > 4? 3 : 20);
    if (attacker->id() == MONS_YELLOW_WASP)
        paralyse_roll += 3;

    if (defender->res_poison() <= 0)
    {
        if (one_chance_in(paralyse_roll))
            defender->paralyse( roll_dice(1, 3) );
        else
            defender->slow_down( roll_dice(1, 3) );
    }
}

void melee_attack::splash_monster_with_acid(int strength)
{
    special_damage += roll_dice(2, 4);
    if (needs_message)
        mprf("%s is splashed with acid.", defender->name(DESC_CAP_THE).c_str());
}

void melee_attack::splash_defender_with_acid(int strength)
{
    if (defender->atype() == ACT_PLAYER)
    {
        mpr("You are splashed with acid!");
        splash_with_acid(strength);
    }
    else
        splash_monster_with_acid(strength);
}

void melee_attack::mons_apply_attack_flavour(const mon_attack_def &attk)
{
    // Most of this is from BWR 4.1.2.
    
    int res = 0;
    switch (attk.flavour)
    {
    default:
        break;
        
    case AF_POISON:
    case AF_POISON_NASTY:
    case AF_POISON_MEDIUM:
    case AF_POISON_STRONG:
        mons_do_poison(attk);
        break;

    case AF_POISON_STR:
        res = defender->res_poison();
        if (res <= 0)
        {
            defender->poison( attacker, roll_dice(1,3) );
            if (one_chance_in(4))
                defender->drain_stat( STAT_STRENGTH, 1 );
        }
        break;

    case AF_ROT:
        if (one_chance_in(20) || (damage_done > 2 && one_chance_in(3)))
            defender->rot( attacker, 2 + random2(3), damage_done > 5 );
        break;

    case AF_DISEASE:
        defender->sicken( 50 + random2(100) );
        break;

    case AF_FIRE:
        if (atk->type == MONS_FIRE_VORTEX)
            atk->hit_points = -10;

        special_damage =
            resist_adjust_damage(defender->res_fire(),
                                 atk->hit_dice + random2(atk->hit_dice));
        if (needs_message && special_damage)
            mprf("%s %s engulfed in flames%s",
                 defender->name(DESC_CAP_THE).c_str(),
                 defender->conj_verb("are").c_str(),
                 special_attack_punctuation().c_str());

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_COLD:
        special_damage =
            resist_adjust_damage(defender->res_cold(),
                                 atk->hit_dice + random2( 2 * atk->hit_dice ));
        if (needs_message && special_damage)
            mprf("%s %s %s!",
                 attacker->name(DESC_CAP_THE).c_str(),
                 attacker->conj_verb("freeze").c_str(),
                 defender->name(DESC_NOCAP_THE).c_str());
        break;

    case AF_ELEC:
        special_damage =
            resist_adjust_damage(
                defender->res_elec(),
                atk->hit_dice + random2( atk->hit_dice / 2 ));

        if (defender->levitates())
            special_damage = special_damage * 2 / 3;

        if (needs_message && special_damage)
            mprf("%s %s %s%s",
                 attacker->name(DESC_CAP_THE).c_str(),
                 attacker->conj_verb("shock").c_str(),
                 defender->name(DESC_NOCAP_THE).c_str(),
                 special_attack_punctuation().c_str());

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Shock damage: %d", special_damage);
#endif
        break;

    case AF_VAMPIRIC:
        if (defender->res_negative_energy() > random2(3))
            break;

        if (defender->stat_hp() < defender->stat_maxhp())
        {
            defender->heal(1 + random2(damage_done), coinflip());

            if (needs_message)
            {
                mprf("%s %s strength from %s injuries!",
                     attacker->name(DESC_CAP_THE).c_str(),
                     attacker->conj_verb("draw").c_str(),
                     defender->name(DESC_NOCAP_YOUR).c_str());
            }

            // 4.1.2 actually drains max hp; we're being nicer and just doing
            // a rot effect.
            if ((damage_done > 6 && one_chance_in(3)) || one_chance_in(20))
            {
                if (defender->atype() == ACT_PLAYER)
                    mprf("You feel less resilient.");
                defender->rot( attacker, 0, coinflip()? 2 : 1 );
            }
        }
        break;

    case AF_DRAIN_STR:
        if ((one_chance_in(20) || (damage_done > 0 && one_chance_in(3)))
            && defender->res_negative_energy() < random2(4))
        {
            defender->drain_stat(STAT_STRENGTH, 1);
        }
        break;

    case AF_DRAIN_DEX:
        if ((one_chance_in(20) || (damage_done > 0 && one_chance_in(3)))
            && defender->res_negative_energy() < random2(4))
        {
            defender->drain_stat(STAT_DEXTERITY, 1);
        }
        break;

    case AF_HUNGER:
        if (defender->holiness() == MH_UNDEAD)
            break;

        if (one_chance_in(20) || (damage_done > 0 && coinflip()))
            defender->make_hungry(400, false);
        break;

    case AF_BLINK:
        if (one_chance_in(3))
        {
            mprf("%s %s!", attacker->name(DESC_CAP_THE).c_str(),
                 attacker->conj_verb("blink").c_str());
            attacker->blink();
        }
        break;

    case AF_CONFUSE:
        if (attk.type == AT_SPORE)
        {
            if (defender->res_poison() > 0)
                break;

            if (--atk->hit_dice <= 0)
                atk->hit_points = -1;

            if (needs_message)
                mprf("%s %s engulfed in a cloud of spores!",
                     defender->name(DESC_CAP_THE).c_str(),
                     defender->conj_verb("are").c_str());
        }

        if (one_chance_in(10)
            || (damage_done > 2 && one_chance_in(3)))
        {
            defender->confuse( 1 + random2( 3 + atk->hit_dice ) );
        }
        break;

    case AF_DRAIN_XP:
        if (attacker->id() == MONS_SPECTRAL_THING && coinflip())
            break;

        if (one_chance_in(30)
            || (damage_done > 5 && coinflip())
            || (attk.damage == 0 && !one_chance_in(3)))
        {
            drain_defender();
        }
        
        break;

    case AF_PARALYSE:
        // Only wasps at the moment.
        wasp_paralyse_defender();
        break;

    case AF_ACID:
        if (attacker->id() == MONS_SPINY_WORM && defender->res_poison() <= 0)
            defender->poison( attacker, 2 + random2(4) );
        splash_defender_with_acid(3);
        break;

    case AF_DISTORT:
        distortion_affects_defender();
        break;

    case AF_RAGE:
        if (!one_chance_in(3) || !defender->can_go_berserk())
            break;
        
        if (needs_message)
            mprf("%s %s %s!",
                 attacker->name(DESC_CAP_THE).c_str(),
                 attacker->conj_verb("infuriate").c_str(),
                 defender->name(DESC_NOCAP_THE).c_str());

        defender->go_berserk(false);
        break;
    }
}

void melee_attack::mons_perform_attack_rounds()
{
    const int nrounds = atk->type == MONS_HYDRA? atk->number : 4;
    const coord_def pos = defender->pos();
    
    for (attack_number = 0; attack_number < nrounds; ++attack_number)
    {
        // Monster went away?
        if (!defender->alive() || defender->pos() != pos)
            break;

        // Monsters hitting themselves get just one round.
        if (attack_number > 0 && attacker == defender)
            break;
        
        const mon_attack_def attk = mons_attack_spec(atk, attack_number);
        if (attk.type == AT_NONE)
            break;

        damage_done = 0;
        mons_set_weapon(attk);
        to_hit = mons_to_hit();

        final_attack_delay = mons_attk_delay();
        if (damage_brand == SPWPN_SPEED)
            final_attack_delay = final_attack_delay / 2 + 1;

        mons_lose_attack_energy(atk, final_attack_delay, attack_number);

        bool shield_blocked = false;
        bool this_round_hit = false;

        if (attacker != defender)
        {
            if (attack_shield_blocked(true))
            {
                shield_blocked = true;
                perceived_attack = true;
                this_round_hit = did_hit = true;
            }
            else
                check_defender_train_dodging();
        }

        if (!shield_blocked)
        {
            const int defender_evasion = defender->melee_evasion(attacker);
            if (attacker == defender
                || test_melee_hit(to_hit, defender_evasion))
            {
                this_round_hit = did_hit = true;
                perceived_attack = true;
                damage_done = mons_calc_damage(attk);
            }
            else
            {
                this_round_hit = false;
                perceived_attack = perceived_attack || attacker_visible;

                if (needs_message)
                    mprf("%s misses %s.",
                         attacker->name(DESC_CAP_THE).c_str(),
                         mons_defender_name().c_str());
            }
        }

        if (damage_done < 1 && this_round_hit && !shield_blocked)
            mons_announce_dud_hit(attk);

        if (damage_done > 0)
        {
            mons_announce_hit(attk);
            check_defender_train_armour();

            if (actor_decapitates_hydra(attacker, defender, damage_done,
                                        attacker->damage_type(attack_number)))
                continue;

            special_damage = 0;
            special_damage_message.clear();

            // Monsters attacking themselves don't get attack flavour, the
            // message sequences look too weird.
            if (attacker != defender)
                mons_apply_attack_flavour(attk);
            
            if (!special_damage_message.empty())
                mprf("%s", special_damage_message.c_str());
            
            defender->hurt(attacker, damage_done + special_damage);
            
            if (!defender->alive() || attacker == defender)
                return;

            special_damage = 0;
            special_damage_message.clear();
            apply_damage_brand();

            if (!special_damage_message.empty())
                mprf("%s", special_damage_message.c_str());

            if (special_damage > 0)
                defender->hurt(attacker, special_damage);
        }
    }
}

bool melee_attack::mons_perform_attack()
{
    if (attacker != defender && mons_self_destructs())
        return (did_hit = perceived_attack = true);

    if (attacker != defender && mons_attack_warded_off())
    {
        perceived_attack = true;
        return (false);
    }

    mons_perform_attack_rounds();
    
    return (did_hit);
}

void melee_attack::mons_check_attack_perceived()
{
    if (!perceived_attack)
        return;

    if (defender->atype() == ACT_PLAYER)
    {
        interrupt_activity(AI_MONSTER_ATTACKS, atk);
            
        // if a friend wants to help, they can attack <monster_attacking>
        if (you.pet_target == MHITNOT)
            you.pet_target = monster_index(atk);
    }
}

bool melee_attack::mons_attack_you()
{
    mons_perform_attack();
    mons_check_attack_perceived();
    return (did_hit);
}

int melee_attack::mons_to_hit()
{
    const int hd_mult = mons_class_flag(atk->type, M_FIGHTER)? 25 : 15;
    int mhit = 18 + atk->hit_dice * hd_mult / 10;
    if (water_attack)
        mhit += 5;

    if (weapon && weapon->base_type == OBJ_WEAPONS)
        mhit += weapon->plus + property(*weapon, PWPN_HIT);

    if (attacker->confused())
        mhit -= 5;

    return (mhit);
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

// Lose attack energy for attacking with a weapon. The monster has already lost
// the base attack cost by this point.
static void mons_lose_attack_energy(monsters *attacker, int wpn_speed,
                                    int which_attack)
{
    // Monsters lose energy only for the first two weapon attacks; subsequent
    // hits are free.
    if (which_attack > 1)
        return;
    
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
    monsters *attacker = &menv[monster_attacking];
    
    if (mons_friendly(attacker) && !mons_is_confused(attacker))
        return;

    melee_attack attk(attacker, &you);
    attk.attack();
}                               // end monster_attack()

bool monsters_fight(int monster_attacking, int monster_attacked)
{
    monsters *attacker = &menv[monster_attacking];
    monsters *defender = &menv[monster_attacked];

    melee_attack attk(attacker, defender);
    return attk.attack();
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
    //   swords are a 50/50 spit; bastard swords are in between.
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
