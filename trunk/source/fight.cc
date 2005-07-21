/*
 *  File:       fight.cc
 *  Summary:    Functions used during combat.
 *  Written by: Linley Henzell
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
#include "view.h"
#include "wpn-misc.h"

#define HIT_WEAK 7
#define HIT_MED 18
#define HIT_STRONG 36
// ... was 5, 12, 21
// how these are used will be replaced by a function in a second ... :P {dlb}

static int weapon_type_modify( int weap, char noise[80], char noise2[80], 
                               int damage );

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

void you_attack(int monster_attacked, bool unarmed_attacks)
{
    struct monsters *defender = &menv[monster_attacked];

    int your_to_hit;
    int damage_done = 0;
    bool hit = false;
    unsigned char stab_bonus = 0;       // this is never negative {dlb}
    int temp_rand;              // for probability determination {dlb}

    const int weapon = you.equip[EQ_WEAPON];
    const bool ur_armed = (weapon != -1);   // compacts code a bit {dlb}
    const bool bearing_shield = (you.equip[EQ_SHIELD] != -1);

    const int melee_brand = player_damage_brand();

    bool water_attack = false;

    char damage_noise[80], damage_noise2[80];

    int heavy_armour = 0;
    char str_pass[ ITEMNAME_SIZE ];

#if DEBUG_DIAGNOSTICS
    char st_prn[ 20 ];
#endif

    FixedVector< char, RA_PROPERTIES > art_proprt;

    if (ur_armed && you.inv[weapon].base_type == OBJ_WEAPONS
                 && is_random_artefact( you.inv[weapon] ))
    {
        randart_wpn_properties( you.inv[weapon], art_proprt );    
    }
    else
    {
        // Probably over protective, but in this code we're going
        // to play it safe. -- bwr
        for (int i = 0; i < RA_PROPERTIES; i++)
            art_proprt[i] = 0;
    }

    // heavy armour modifiers for shield borne
    if (bearing_shield)
    {
        switch (you.inv[you.equip[EQ_SHIELD]].sub_type)
        {
        case ARM_SHIELD:
            if (you.skills[SK_SHIELDS] < random2(7))
                heavy_armour++;
            break;
        case ARM_LARGE_SHIELD:
            if ((you.species >= SP_OGRE && you.species <= SP_OGRE_MAGE)
                || player_genus(GENPC_DRACONIAN))
            {
                if (you.skills[SK_SHIELDS] < random2(13))
                    heavy_armour++;     // was potentially "+= 3" {dlb}
            }
            else
            {
                for (int i = 0; i < 3; i++)
                {
                    if (you.skills[SK_SHIELDS] < random2(13))
                        heavy_armour += random2(3);
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

        if (ev_pen < 0 && random2(you.skills[SK_ARMOUR]) < abs( ev_pen ))
            heavy_armour += random2( abs(ev_pen) );
    }

    // ??? what is the reasoning behind this ??? {dlb}
    // My guess is that its supposed to encourage monk-style play -- bwr
    if (!ur_armed)
        heavy_armour *= (coinflip() ? 3 : 2);

    // Calculate the following two flags in advance
    // to know what player does this combat turn:
    bool can_do_unarmed_combat = false;

    if (you.burden_state == BS_UNENCUMBERED
        && random2(20) < you.skills[SK_UNARMED_COMBAT]
        && random2(1 + heavy_armour) < 2)
    {
        can_do_unarmed_combat = true;
    }

    // if we're not getting potential unarmed attacks, and not wearing a
    // shield, and have a suitable uncursed weapon we get the bonus.
    bool use_hand_and_a_half_bonus = false;

    int wpn_skill = SK_UNARMED_COMBAT;
    int hands_reqd = HANDS_ONE_HANDED;

    if (weapon != -1)
    {
        wpn_skill = weapon_skill( you.inv[weapon].base_type, 
                                  you.inv[weapon].sub_type );

        hands_reqd = hands_reqd_for_weapon( you.inv[weapon].base_type,
                                            you.inv[weapon].sub_type );
    }

    if (unarmed_attacks
        && !can_do_unarmed_combat
        && !bearing_shield && ur_armed
        && item_uncursed( you.inv[ weapon ] )
        && hands_reqd == HANDS_ONE_OR_TWO_HANDED)
    {
        // currently: +1 dam, +1 hit, -1 spd (loosly)
        use_hand_and_a_half_bonus = true;
    }

/*
 **************************************************************************
 *                                                                        *
 *  IMPORTANT: When altering damage routines, must also change in ouch.cc *
 *  for saving of player ghosts.                                          *
 *                                                                        *
 **************************************************************************
 */
    bool helpless = mons_flag(defender->type, M_NO_EXP_GAIN);

    if (mons_friendly(defender))
        naughty(NAUGHTY_ATTACK_FRIEND, 5);

    if (you.pet_target == MHITNOT) 
        you.pet_target = monster_attacked;

    // fumbling in shallow water <early return>:
    if (player_in_water() && !player_is_swimming())
    {
        if (random2(you.dex) < 4 || one_chance_in(5))
        {
            mpr("Unstable footing causes you to fumble your attack.");
            return;
        }
    }

    // wet merfolk
    if (player_is_swimming()
        // monster not a water creature
        && monster_habitat( defender->type ) != DNGN_DEEP_WATER
        && !mons_flag( defender->type, M_AMPHIBIOUS )
        // monster in water
        && (grd[defender->x][defender->y] == DNGN_SHALLOW_WATER
            || grd[defender->x][defender->y] == DNGN_DEEP_WATER)
        // monster not flying
        && !mons_flies( defender ))
    {
        water_attack = true;
    }

    // preliminary to_hit modifications:
    your_to_hit = 15 + (calc_stat_to_hit_base() / 2);

    if (water_attack)
        your_to_hit += 5;

    if (wearing_amulet(AMU_INACCURACY))
        your_to_hit -= 5;

    // if you can't see yourself, you're a little less acurate.
    if (you.invis && !player_see_invis())
        your_to_hit -= 5;

    your_to_hit += random2(1 + you.skills[SK_FIGHTING]);

    if (ur_armed)
    {
        if (wpn_skill)
            your_to_hit += random2(you.skills[wpn_skill] + 1);
    }
    else                        // ...you must be unarmed
    {
        your_to_hit += (you.species == SP_TROLL
                        || you.species == SP_GHOUL) ? 4 : 2;

        your_to_hit += random2(1 + you.skills[SK_UNARMED_COMBAT]);
    }

    // energy expenditure in terms of food:
    make_hungry(3, true);

    if (ur_armed
        && you.inv[ weapon ].base_type == OBJ_WEAPONS
        && is_random_artefact( you.inv[ weapon ] ))
    {
        if (art_proprt[RAP_ANGRY] >= 1)
        {
            if (random2(1 + art_proprt[RAP_ANGRY]))
            {
                go_berserk(false);
            }
        }
    }

    switch (you.special_wield)
    {
    case SPWLD_TROG:
        if (coinflip())
            go_berserk(false);
        break;

    case SPWLD_WUCAD_MU:
        if (one_chance_in(9))
        {
            miscast_effect( SPTYP_DIVINATION, random2(9), random2(70), 100, 
                            "the Staff of Wucad Mu" );
        }
        break;
    default:
        break;
    }

    if (you.mutation[MUT_BERSERK])
    {
        if (random2(100) < (you.mutation[MUT_BERSERK] * 10) - 5)
            go_berserk(false);
    }

    if (ur_armed)
    {
        if (you.inv[ weapon ].base_type == OBJ_WEAPONS)
        {
            // there was some stupid conditional here that applied this
            // "if (plus >= 0)" and "else" that .. which is all
            // cases (d'oh!) {dlb}
            your_to_hit += you.inv[ weapon ].plus;
            your_to_hit += property( you.inv[ weapon ], PWPN_HIT );

            if (cmp_equip_race( you.inv[ weapon ], ISFLAG_ELVEN )
                && player_genus(GENPC_ELVEN))
            {
                your_to_hit += (coinflip() ? 2 : 1);
            }
        }
        else if (item_is_staff( you.inv[ weapon ] ))
        {
            /* magical staff */ 
            your_to_hit += property( you.inv[ weapon ], PWPN_HIT );
        }
    }

    your_to_hit += slaying_bonus(PWPN_HIT);     // see: player.cc

    if (you.hunger_state == HS_STARVING)
        your_to_hit -= 3;

    your_to_hit -= heavy_armour;

#if DEBUG_DIAGNOSTICS
    int roll_hit = your_to_hit;
#endif

    // why does this come here and not later? {dlb}
    // apparently to give the following pluses more significance -- bwr
    your_to_hit = random2(your_to_hit);

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "to hit die: %d; rolled value: %d",
              roll_hit, your_to_hit );

    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (use_hand_and_a_half_bonus)
        your_to_hit += random2(3);

    if (ur_armed && wpn_skill == SK_SHORT_BLADES && you.sure_blade)
        your_to_hit += 5 + random2limit( you.sure_blade, 10 );

    int damage = 1;

    if (!ur_armed)              // empty-handed
    {
        damage = 3;

        if (you.confusing_touch)
        {
            // just trying to touch is easier that trying to damage
            // special_brand = SPWPN_CONFUSE;
            your_to_hit += random2(you.dex);
            // no base hand damage while using this spell
            damage = 0;
        }

        // if (you.mutation[MUT_DRAIN_LIFE])
            // special_brand = SPWPN_DRAINING;

        if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
        {
            switch (you.attribute[ATTR_TRANSFORMATION])
            {
            case TRAN_SPIDER:
                damage = 5;
                // special_brand = SPWPN_VENOM;
                your_to_hit += random2(10);
                break;
            case TRAN_ICE_BEAST:
                damage = 12;
                // special_brand = SPWPN_FREEZING;
                your_to_hit += random2(10);
                break;
            case TRAN_BLADE_HANDS:
                damage = 12 + (you.strength / 4) + (you.dex / 4);
                your_to_hit += random2(12);
                break;
            case TRAN_STATUE:
                damage = 12 + you.strength;
                your_to_hit += random2(9);
                break;
            case TRAN_SERPENT_OF_HELL:
            case TRAN_DRAGON:
                damage = 20 + you.strength;
                your_to_hit += random2(10);
                break;
            case TRAN_LICH:
                damage = 5;
                // special_brand = SPWPN_DRAINING;
                your_to_hit += random2(10);
                break;
            case TRAN_AIR:
                damage = 0;
                your_to_hit = 0;
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
    }
    else
    {
        if (you.inv[ weapon ].base_type == OBJ_WEAPONS
            || item_is_staff( you.inv[ weapon ] ))
        {
            damage = property( you.inv[ weapon ], PWPN_DAMAGE );
        }
    }

#if DEBUG_DIAGNOSTICS
    const int base_damage = damage;
#endif

    //jmf: check for backlight enchantment
    if (mons_has_ench(defender, ENCH_BACKLIGHT_I, ENCH_BACKLIGHT_IV))
       your_to_hit += 2 + random2(8);

    int weapon_speed2 = 10;
    int min_speed = 3;

    if (ur_armed)
    {
        if (you.inv[ weapon ].base_type == OBJ_WEAPONS
            || item_is_staff( you.inv[ weapon ] ))
        {
            weapon_speed2 = property( you.inv[ weapon ], PWPN_SPEED );
            weapon_speed2 -= you.skills[ wpn_skill ] / 2;

            min_speed = property( you.inv[ weapon ], PWPN_SPEED ) / 2;

            // Short blades can get up to at least unarmed speed.
            if (wpn_skill == SK_SHORT_BLADES && min_speed > 5)
                min_speed = 5;

            // Using both hands can get a weapon up to speed 7
            if ((hands_reqd == HANDS_TWO_HANDED || use_hand_and_a_half_bonus)
                && min_speed > 7)
            {
                min_speed = 7;
            }

            // never go faster than speed 3 (ie 3 attacks per round)
            if (min_speed < 3)
                min_speed = 3;

            // Hand and a half bonus only helps speed up to a point, any more
            // than speed 10 must come from skill and the weapon
            if (use_hand_and_a_half_bonus && weapon_speed2 > 10)
                weapon_speed2--;

            // apply minimum to weapon skill modification
            if (weapon_speed2 < min_speed)
                weapon_speed2 = min_speed;

            if (you.inv[weapon].base_type == OBJ_WEAPONS
                && melee_brand == SPWPN_SPEED)
            {
                weapon_speed2 = (weapon_speed2 + 1) / 2;
            }
        }
    }
    else
    {
        // Unarmed speed
        if (you.burden_state == BS_UNENCUMBERED
            && one_chance_in(heavy_armour + 1))
        {
            weapon_speed2 = 10 - you.skills[SK_UNARMED_COMBAT] / 3;

            if (weapon_speed2 < 4)
                weapon_speed2 = 4;
        }
    }

    if (bearing_shield)
    {
        switch (you.inv[you.equip[EQ_SHIELD]].sub_type)
        {
        case ARM_SHIELD:
            weapon_speed2++;
            break;
        case ARM_LARGE_SHIELD:
            weapon_speed2 += 2;
            break;
        }
    }

    // Never allow anything faster than 3 to get through... three attacks
    // per round is enough... 5 or 10 is just silly. -- bwr
    if (weapon_speed2 < 3)
        weapon_speed2 = 3;

    you.time_taken *= weapon_speed2;
    you.time_taken /= 10;

    if (you.time_taken < 1)
        you.time_taken = 1;

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "Weapon speed: %d; min: %d; speed: %d; attack time: %d", 
              weapon_speed2, min_speed, weapon_speed2, you.time_taken );

    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    // DO STABBING

    bool stabAttempt = false;
    bool rollNeeded = true;

    // This ordering is important!

    // not paying attention (but not batty)
    if (defender->foe != MHITYOU && !testbits(defender->flags, MF_BATTY))
    {
        stabAttempt = true;
        stab_bonus = 3;
    }

    // confused (but not perma-confused)
    if (mons_has_ench(defender, ENCH_CONFUSION) 
        && !mons_flag(defender->type, M_CONFUSED))
    {
        stabAttempt = true;
        stab_bonus = 2;
    }

    // fleeing
    if (defender->behaviour == BEH_FLEE)
    {
        stabAttempt = true;
        stab_bonus = 2;
    }

    // sleeping
    if (defender->behaviour == BEH_SLEEP)
    {
        stabAttempt = true;
        rollNeeded = false;
        stab_bonus = 1;
    }

    // helpless (plants, etc)
    if (helpless)
        stabAttempt = false;

    // see if we need to roll against dexterity / stabbing
    if (stabAttempt && rollNeeded)
        stabAttempt = (random2(200) <= you.skills[SK_STABBING] + you.dex);

    // check for invisibility - no stabs on invisible monsters.
    if (!player_monster_visible( defender ))
    {
        stabAttempt = false;
        stab_bonus = 0;
    }

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "your to-hit: %d; defender EV: %d", 
              your_to_hit, defender->evasion );

    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if ((your_to_hit >= defender->evasion || one_chance_in(30))
        || ((defender->speed_increment <= 60
             || defender->behaviour == BEH_SLEEP)
            && !one_chance_in(10 + you.skills[SK_STABBING])))
    {
        hit = true;
        int dammod = 78;

        const int dam_stat_val = calc_stat_to_dam_base();

        if (dam_stat_val > 11)
            dammod += (random2(dam_stat_val - 11) * 2);
        else if (dam_stat_val < 9)
            dammod -= (random2(9 - dam_stat_val) * 3);

        damage *= dammod;       //random2(you.strength);
        damage /= 78;

#if DEBUG_DIAGNOSTICS
        const int str_damage = damage;
#endif

        // Yes, this isn't the *2 damage that monsters get, but since
        // monster hps and player hps are different (as are the sizes
        // of the attacks) it just wouldn't be fair to give merfolk
        // that gross a potential... still they do get something for
        // making use of the water.  -- bwr
        if (water_attack)
            damage += random2avg(10,2);

#if DEBUG_DIAGNOSTICS
        const int water_damage = damage;
#endif

        //  apply damage bonus from ring of slaying
        // (before randomization -- some of these rings
        //  are stupidly powerful) -- GDL
        damage += slaying_bonus(PWPN_DAMAGE);

#if DEBUG_DIAGNOSTICS
        const int slay_damage = damage;

        snprintf( info, INFO_SIZE, 
                  "melee base: %d; str: %d; water: %d; to-dam: %d", 
                  base_damage, str_damage, water_damage, slay_damage );

        mpr( info, MSGCH_DIAGNOSTICS );
#endif

        damage_done = random2(damage);

#if DEBUG_DIAGNOSTICS
        const int roll_damage = damage_done;
#endif

        if (ur_armed && (you.inv[ weapon ].base_type == OBJ_WEAPONS
                            || item_is_staff( you.inv[ weapon ] )))
        {
            damage_done *= 25 + (random2( you.skills[ wpn_skill ] + 1 ));
            damage_done /= 25;
        }

#if DEBUG_DIAGNOSTICS
        const int skill_damage = damage_done;
#endif

        damage_done *= 30 + (random2(you.skills[SK_FIGHTING] + 1));
        damage_done /= 30;

#if DEBUG_DIAGNOSTICS
        const int fight_damage = damage_done;
#endif

        if (you.might > 1)
            damage_done += 1 + random2(10);

        if (you.hunger_state == HS_STARVING)
            damage_done -= random2(5);

#if DEBUG_DIAGNOSTICS
        const int preplus_damage = damage_done;
        int plus_damage = damage_done;
        int bonus_damage = damage_done;
#endif

        if (ur_armed && you.inv[ weapon ].base_type == OBJ_WEAPONS)
        {
            int hoggl = you.inv[ weapon ].plus2;

            damage_done += (hoggl > -1) ? (random2(1 + hoggl)) : (hoggl);

#if DEBUG_DIAGNOSTICS
            plus_damage = damage_done;
#endif

            // removed 2-handed weapons from here... their "bonus" is
            // already included in the damage stat for the weapon -- bwr
            if (use_hand_and_a_half_bonus)
                damage_done += random2(3);

            if (cmp_equip_race( you.inv[ weapon ], ISFLAG_DWARVEN )
                && player_genus(GENPC_DWARVEN))
            {
                damage_done += random2(3);
            }

            if (cmp_equip_race( you.inv[ weapon ], ISFLAG_ORCISH )
                && you.species == SP_HILL_ORC && coinflip())
            {
                damage_done++;
            }

#if DEBUG_DIAGNOSTICS
            bonus_damage = damage_done;
#endif

            if (!launches_things( you.inv[ weapon ].sub_type )
                && item_not_ident( you.inv[ weapon ], ISFLAG_KNOW_PLUSES )
                && random2(100) < you.skills[ wpn_skill ])
            {
                set_ident_flags( you.inv[ weapon ], ISFLAG_KNOW_PLUSES );
                strcpy(info, "You are wielding ");
                in_name( weapon , DESC_NOCAP_A, str_pass );
                strcat(info, str_pass);
                strcat(info, ".");
                mpr(info);
                more();
                you.wield_change = true;
            }
        }

        // The stabbing message looks better here:
        if (stabAttempt)
        {
            // construct reasonable message
            stab_message( defender, stab_bonus );

            exercise(SK_STABBING, 1 + random2avg(5, 4));

            if (mons_holiness(defender->type) == MH_NATURAL
                || mons_holiness(defender->type) == MH_HOLY)
            {
                naughty(NAUGHTY_STABBING, 4);
            }
        }
        else
        {
            stab_bonus = 0;
            // ok.. if you didn't backstab, you wake up the neighborhood.
            // I can live with that.
            alert_nearby_monsters();
        }

#if DEBUG_DIAGNOSTICS
        int stab_damage = damage_done;
#endif

        if (stab_bonus)
        {
            // lets make sure we have some damage to work with...
            if (damage_done < 1)
                damage_done = 1;

            if (defender->behaviour == BEH_SLEEP)
            {
                // Sleeping moster wakes up when stabbed but may be groggy
                if (random2(200) <= you.skills[SK_STABBING] + you.dex)
                {
                    unsigned int stun = random2( you.dex + 1 );

                    if (defender->speed_increment > stun)
                        defender->speed_increment -= stun;
                    else
                        defender->speed_increment = 0;
                }
            }

            switch (wpn_skill)
            {
            case SK_SHORT_BLADES:
                {
                    int bonus = (you.dex * (you.skills[SK_STABBING] + 1)) / 5;

                    if (you.inv[ weapon ].sub_type != WPN_DAGGER)
                        bonus /= 2;

                    bonus = stepdown_value( bonus, 10, 10, 30, 30 );

                    damage_done += bonus;
                }
                // fall through
            case SK_LONG_SWORDS:
                damage_done *= 10 + you.skills[SK_STABBING] /
                    (stab_bonus + (wpn_skill == SK_SHORT_BLADES ? 0 : 1));
                damage_done /= 10;
                // fall through
            default:
                damage_done *= 12 + you.skills[SK_STABBING] / stab_bonus;
                damage_done /= 12;
            }

#if DEBUG_DIAGNOSTICS
            stab_damage = damage_done;
#endif

            // when stabbing we can get by some of the armour
            if (defender->armour_class > 0)
            {
                int ac = defender->armour_class
                            - random2( you.skills[SK_STABBING] / stab_bonus );

                if (ac > 0)
                    damage_done -= random2(1 + ac);
            }
        }
        else
        {
            // apply AC normally
            if (defender->armour_class > 0)
                damage_done -= random2(1 + defender->armour_class);
        }

#if DEBUG_DIAGNOSTICS
        snprintf( info, INFO_SIZE, 
                  "melee roll: %d; skill: %d; fight: %d; pre wpn plus: %d",
                  roll_damage, skill_damage, fight_damage, preplus_damage );

        mpr( info, MSGCH_DIAGNOSTICS );

        snprintf( info, INFO_SIZE, 
                  "melee plus: %d; bonus: %d; stab: %d; post AC: %d",
                  plus_damage, bonus_damage, stab_damage, damage_done );

        mpr( info, MSGCH_DIAGNOSTICS );
#endif

        // This doesn't actually modify damage -- bwr
        damage_done = weapon_type_modify( weapon, damage_noise, damage_noise2, 
                                          damage_done );

        if (damage_done < 0)
            damage_done = 0;

        // always upset monster regardless of damage
        behaviour_event(defender, ME_WHACK, MHITYOU);

        if (hurt_monster(defender, damage_done))
        {
            if (ur_armed && wpn_skill)
            {
                if (!helpless || you.skills[ wpn_skill ] < 2)
                    exercise( wpn_skill, 1 );
            }
            else
            {
                if (!helpless || you.skills[SK_UNARMED_COMBAT] < 2)
                    exercise(SK_UNARMED_COMBAT, 1);
            }

            if ((!helpless || you.skills[SK_FIGHTING] < 2)
                && one_chance_in(3))
            {
                exercise(SK_FIGHTING, 1);
            }
        }

        if (defender->hit_points < 1)
        {
#if DEBUG_DIAGNOSTICS
            /* note: doesn't take account of special weapons etc */
            snprintf( info, INFO_SIZE, "Hit for %d.", damage_done );
            mpr( info, MSGCH_DIAGNOSTICS );
#endif
            if (ur_armed && melee_brand == SPWPN_VAMPIRICISM)
            {
                if (mons_holiness(defender->type) == MH_NATURAL
                    && damage_done > 0 && you.hp < you.hp_max
                    && !one_chance_in(5))
                {
                    mpr("You feel better.");

                    // more than if not killed
                    inc_hp(1 + random2(damage_done), false);

                    if (you.hunger_state != HS_ENGORGED)
                        lessen_hunger(30 + random2avg(59, 2), true);

                    naughty(NAUGHTY_NECROMANCY, 2);
                }
            }

            monster_die(defender, KILL_YOU, 0);

            if (defender->type == MONS_GIANT_SPORE)
            {
                snprintf( info, INFO_SIZE, "You %s the giant spore.", damage_noise);
                mpr(info);
            }
            else if (defender->type == MONS_BALL_LIGHTNING)
            {
                snprintf( info, INFO_SIZE, "You %s the ball lightning.", damage_noise);
                mpr(info);
            }
            return;
        }

        if (damage_done < 1 && player_monster_visible( defender ))
        {
            hit = true;

            snprintf( info, INFO_SIZE, "You %s ", damage_noise);
            strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
            strcat(info, ", but do no damage.");
            mpr(info);
        }
    }
    else
    {
        hit = false;

        // upset only non-sleeping monsters if we missed
        if (defender->behaviour != BEH_SLEEP)
            behaviour_event( defender, ME_WHACK, MHITYOU );

        if ((your_to_hit + heavy_armour / 2) >= defender->evasion)
            strcpy(info, "Your armour prevents you from hitting ");
        else
            strcpy(info, "You miss ");

        strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
        strcat(info, ".");
        mpr(info);
    }

    if (hit && damage_done > 0
        || (hit && damage_done < 1 && mons_has_ench(defender,ENCH_INVIS)))
    {
        strcpy(info, "You ");
        strcat(info, damage_noise);
        strcat(info, " ");
        strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
        strcat(info, damage_noise2);

#if DEBUG_DIAGNOSTICS
        strcat( info, " for " );
        /* note: doesn't take account of special weapons etc */
        itoa( damage_done, st_prn, 10 );
        strcat( info, st_prn );
#endif
        if (damage_done < HIT_WEAK)
            strcat(info, ".");
        else if (damage_done < HIT_MED)
            strcat(info, "!");
        else if (damage_done < HIT_STRONG)
            strcat(info, "!!");
        else
            strcat(info, "!!!");

        mpr(info);

        if (mons_holiness(defender->type) == MH_HOLY)
            done_good(GOOD_KILLED_ANGEL_I, 1);

        if (you.special_wield == SPWLD_TORMENT)
        {
            torment(you.x_pos, you.y_pos);
            naughty(NAUGHTY_UNHOLY, 5);
        }

        if (you.special_wield == SPWLD_ZONGULDROK
            || you.special_wield == SPWLD_CURSE)
        {
            naughty(NAUGHTY_NECROMANCY, 3);
        }
    }

    if (defender->type == MONS_JELLY
        || defender->type == MONS_BROWN_OOZE
        || defender->type == MONS_ACID_BLOB
        || defender->type == MONS_ROYAL_JELLY)
    {
        weapon_acid(5);
    }

    /* remember, damage_done is still useful! */
    if (hit)
    {
        int specdam = 0;

        if (ur_armed 
            && you.inv[ weapon ].base_type == OBJ_WEAPONS
            && is_demonic( you.inv[ weapon ].sub_type ))
        {
            naughty(NAUGHTY_UNHOLY, 1);
        }

        if (mons_holiness(defender->type) == MH_HOLY)
            naughty(NAUGHTY_ATTACK_HOLY, defender->hit_dice);

        if (defender->type == MONS_HYDRA)
        {
            const int dam_type = player_damage_type();
            const int wpn_brand = player_damage_brand();

            if ((dam_type == DVORP_SLICING || dam_type == DVORP_CHOPPING)
                && damage_done > 0
                && (damage_done >= 4 || wpn_brand == SPWPN_VORPAL || coinflip()))
            {
                defender->number--;

                temp_rand = random2(4);
                const char *const verb = (temp_rand == 0) ? "slice" :
                                         (temp_rand == 1) ? "lop" :
                                         (temp_rand == 2) ? "chop" : "hack";

                if (defender->number < 1)
                {
                    snprintf( info, INFO_SIZE, "You %s %s's last head off!",
                              verb, ptr_monam(defender, DESC_NOCAP_THE) );
                    mpr( info );
                              
                    defender->hit_points = -1; 
                }
                else
                {
                    snprintf( info, INFO_SIZE, "You %s one of %s's heads off!",
                              verb, ptr_monam(defender, DESC_NOCAP_THE) );
                    mpr( info );

                    if (wpn_brand == SPWPN_FLAMING) 
                        mpr( "The flame cauterises the wound!" );
                    else if (defender->number < 19)
                    {
                        simple_monster_message( defender, " grows two more!" );
                        defender->number += 2;
                        heal_monster( defender, 8 + random2(8), true );
                    }
                }

                // if the hydra looses a head:
                // - it's dead if it has none remaining (HP set to -1)
                // - flame used to cauterise doesn't do extra damage
                // - ego weapons do their additional damage to the 
                //   hydra's decapitated head, so it's ignored.
                //
                // ... and so we skip the special damage.
                goto mons_dies;
            }
        }

        // jmf: BEGIN STAFF HACK
        // How much bonus damage will a staff of <foo> do?
        // FIXME: make these not macros. inline functions?
        // actually, it will all be pulled out and replaced by functions -- {dlb}
        //
        // This is similar to the previous, in both value and distribution, except
        // that instead of just SKILL, its now averaged with Evocations. -- bwr
#define STAFF_DAMAGE(SKILL) (roll_dice( 3, 1 + (you.skills[(SKILL)] + you.skills[SK_EVOCATIONS]) / 12 ))
                                    
#define STAFF_COST 2

        // magic staves have their own special damage
        if (ur_armed && item_is_staff( you.inv[weapon] )) 
        {
            specdam = 0;

            if (you.magic_points >= STAFF_COST
                && random2(20) <= you.skills[SK_EVOCATIONS])
            {
                switch (you.inv[weapon].sub_type)
                {
                case STAFF_AIR:
                    if (damage_done + you.skills[SK_AIR_MAGIC] > random2(30))
                    {
                        if (mons_res_elec(defender))
                            break;

                        specdam = STAFF_DAMAGE(SK_AIR_MAGIC);

                        if (specdam)
                        {
                            snprintf( info, INFO_SIZE, "%s is jolted!",
                                      ptr_monam(defender, DESC_CAP_THE) );
                            mpr(info);
                        }
                    }
                    break;

                case STAFF_COLD:    // FIXME: I don't think I used these right ...
                    if (mons_res_cold(defender) > 0)
                        break;

                    specdam = STAFF_DAMAGE(SK_ICE_MAGIC);

                    if (mons_res_cold(defender) < 0)
                        specdam += STAFF_DAMAGE(SK_ICE_MAGIC);

                    if (specdam)
                    {

                        snprintf( info, INFO_SIZE, "You freeze %s!",
                                  ptr_monam(defender, DESC_NOCAP_THE) );
                        mpr(info);
                    }
                    break;

                case STAFF_EARTH:
                    if (mons_flies(defender))
                        break;      //jmf: lame, but someone ought to resist

                    specdam = STAFF_DAMAGE(SK_EARTH_MAGIC);

                    if (specdam) 
                    {
                        snprintf( info, INFO_SIZE, "You crush %s!",
                                  ptr_monam(defender, DESC_NOCAP_THE) );
                        mpr(info);
                    }
                    break;

                case STAFF_FIRE:
                    if (mons_res_fire(defender) > 0)
                        break;

                    specdam = STAFF_DAMAGE(SK_FIRE_MAGIC);

                    if (mons_res_fire(defender) < 0)
                        specdam += STAFF_DAMAGE(SK_FIRE_MAGIC);

                    if (specdam)
                    {
                        snprintf( info, INFO_SIZE, "You burn %s!",
                                  ptr_monam(defender, DESC_NOCAP_THE) );
                        mpr(info);
                    }
                    break;

                case STAFF_POISON:
                    // cap chance at 30% -- let staff of Olgreb shine
                    temp_rand = damage_done + you.skills[SK_POISON_MAGIC];

                    if (temp_rand > 30)
                        temp_rand = 30;

                    if (random2(100) < temp_rand)
                        poison_monster(defender, true);
                    break;

                case STAFF_DEATH:
                    if (mons_res_negative_energy(defender) > 0)
                        break;

                    if (random2(8) <= you.skills[SK_NECROMANCY])
                    {
                        specdam = STAFF_DAMAGE(SK_NECROMANCY);

                        if (specdam)
                        {
                            snprintf( info, INFO_SIZE, "%s convulses in agony!",
                                      ptr_monam(defender, DESC_CAP_THE) );
                            mpr(info);

                            naughty(NAUGHTY_NECROMANCY, 4);
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
            }

            if (specdam > 0)
            {
                dec_mp(STAFF_COST);

                if (item_not_ident( you.inv[weapon], ISFLAG_KNOW_TYPE ))
                {
                    set_ident_flags( you.inv[weapon], ISFLAG_KNOW_TYPE );
                    strcpy(info, "You are wielding ");
                    in_name( weapon, DESC_NOCAP_A, str_pass);
                    strcat(info, str_pass);
                    strcat(info, ".");
                    mpr(info);
                    more();
                    you.wield_change = true;
                }
            }
#undef STAFF_DAMAGE
#undef STAFF_COST
// END STAFF HACK
        } 
        else
        {
            // handle special brand damage (unarmed or armed non-staff ego):
            int res = 0;

            switch (melee_brand)
            {
            case SPWPN_NORMAL:
                break;

            case SPWPN_FLAMING:
                specdam = 0;

                res = mons_res_fire(defender);
                if (ur_armed && you.inv[weapon].special == SPWPN_SWORD_OF_CEREBOV)
                {
                    if (res < 3 && res > 0)
                        res = 0;
                    else if (res == 0)
                        res = -1;
                }

                if (res == 0)
                    specdam = random2(damage_done) / 2 + 1;
                else if (res < 0)
                    specdam = random2(damage_done) + 1;

                if (specdam)
                {
                    strcpy(info, "You burn ");
                    strcat(info, ptr_monam(defender, DESC_NOCAP_THE));

                    if (specdam < 3)
                        strcat(info, ".");
                    else if (specdam < 7)
                        strcat(info, "!");
                    else
                        strcat(info, "!!");

                    mpr(info);
                }
                break;

            case SPWPN_FREEZING:
                specdam = 0;

                res = mons_res_cold(defender);
                if (res == 0)
                    specdam = 1 + random2(damage_done) / 2;
                else if (res < 0)
                    specdam = 1 + random2(damage_done);

                if (specdam)
                {
                    strcpy(info, "You freeze ");
                    strcat(info, ptr_monam(defender, DESC_NOCAP_THE));

                    if (specdam < 3)
                        strcat(info, ".");
                    else if (specdam < 7)
                        strcat(info, "!");
                    else
                        strcat(info, "!!");

                    mpr(info);
                }
                break;

            case SPWPN_HOLY_WRATH:
                // there should be a case in here for holy monsters,
                // see elsewhere in fight.cc {dlb}
                specdam = 0;
                switch (mons_holiness(defender->type))
                {
                case MH_UNDEAD:
                    specdam = 1 + random2(damage_done);
                    break;

                case MH_DEMONIC:
                    specdam = 1 + (random2(damage_done * 15) / 10);
                    break;
                }
                break;


            case SPWPN_ELECTROCUTION:
                specdam = 0;

                if (mons_flies(defender))
                    break;
                else if (mons_res_elec(defender) > 0)
                    break;
                else if (one_chance_in(3))
                {
                    mpr("There is a sudden explosion of sparks!");
                    specdam = random2avg(28, 3);
                }
                break;

            case SPWPN_ORC_SLAYING:
                if (mons_charclass(defender->type) == MONS_ORC)
                    hurt_monster(defender, 1 + random2(damage_done));
                break;

            case SPWPN_VENOM:
                if (!one_chance_in(4))
                    poison_monster(defender, true);
                break;

            case SPWPN_DRAINING:
                if (mons_res_negative_energy(defender) > 0 || one_chance_in(3))
                    break;

                strcpy(info, "You drain ");
                strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                strcat(info, "!");
                mpr(info);

                if (one_chance_in(5))
                    defender->hit_dice--;

                defender->max_hit_points -= 2 + random2(3);
                defender->hit_points -= 2 + random2(3);

                if (defender->hit_points >= defender->max_hit_points)
                    defender->hit_points = defender->max_hit_points;

                if (defender->hit_dice < 1)
                    defender->hit_points = 0;

                specdam = 1 + (random2(damage_done) / 2);
                naughty( NAUGHTY_NECROMANCY, 2 );
                break;

                /* 9 = speed - done before */

            case SPWPN_VORPAL:
                specdam = 1 + random2(damage_done) / 2;
                break;

            case SPWPN_VAMPIRICISM:
                specdam = 0;        // NB: does no extra damage

                if (mons_holiness(defender->type) != MH_NATURAL)
                    break;
                else if (mons_res_negative_energy(defender) > 0)
                    break;
                else if (damage_done < 1)
                    break;
                else if (you.hp == you.hp_max || one_chance_in(5))
                    break;

                mpr("You feel better.");

                // thus is probably more valuable on larger weapons?
                if (ur_armed 
                    && is_fixed_artefact( you.inv[weapon] ) 
                    && you.inv[weapon].special == SPWPN_VAMPIRES_TOOTH)
                {
                    inc_hp(damage_done, false);
                }
                else
                    inc_hp(1 + random2(damage_done), false);

                if (you.hunger_state != HS_ENGORGED)
                    lessen_hunger(random2avg(59, 2), true);

                naughty( NAUGHTY_NECROMANCY, 2 );
                break;

            case SPWPN_DISRUPTION:
                specdam = 0;
                if (mons_holiness(defender->type) == MH_UNDEAD && !one_chance_in(3))
                {
                    simple_monster_message(defender, " shudders.");
                    specdam += random2avg((1 + (damage_done * 3)), 3);
                }
                break;

            case SPWPN_PAIN:
                specdam = 0;
                if (mons_res_negative_energy(defender) <= 0
                    && random2(8) <= you.skills[SK_NECROMANCY])
                {
                    simple_monster_message(defender, " convulses in agony.");
                    specdam += random2( 1 + you.skills[SK_NECROMANCY] );
                }
                naughty(NAUGHTY_NECROMANCY, 4);
                break;

            case SPWPN_DISTORTION:
                //jmf: blink frogs *like* distortion
                // I think could be amended to let blink frogs "grow" like
                // jellies do {dlb}
                if (defender->type == MONS_BLINK_FROG)
                {
                    if (one_chance_in(5))
                    {
                        simple_monster_message( defender,
                            " basks in the translocular energy." );
                        heal_monster(defender, 1 + random2avg(7, 2), true); // heh heh
                    }
                    break;
                }

                if (one_chance_in(3))
                {
                    strcpy(info, "Space bends around ");
                    strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                    strcat(info, ".");
                    mpr(info);
                    specdam += 1 + random2avg(7, 2);
                    break;
                }

                if (one_chance_in(3))
                {
                    strcpy(info, "Space warps horribly around ");
                    strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                    strcat(info, "!");
                    mpr(info);
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
                    monster_die(defender, KILL_RESET, 0);
                    return;
                }
                break;

            case SPWPN_CONFUSE:
                {
                    // declaring these just to pass to the enchant function
                    struct bolt beam_temp;
                    beam_temp.flavour = BEAM_CONFUSION;

                    mons_ench_f2( defender, beam_temp );

                    you.confusing_touch -= random2(20);

                    if (you.confusing_touch < 1)
                        you.confusing_touch = 1;
                }
                break;
            }                       /* end switch */
        }

        /* remember, the hydra function sometimes skips straight to mons_dies */
#if DEBUG_DIAGNOSTICS
        snprintf( info, INFO_SIZE, "brand: %d; melee special damage: %d", 
                  melee_brand, specdam );
        mpr( info, MSGCH_DIAGNOSTICS );
#endif

        hurt_monster( defender, specdam );
    } // end if (hit)

  mons_dies:
    if (defender->hit_points < 1)
    {
        monster_die(defender, KILL_YOU, 0);
        return;
    }

    if (unarmed_attacks)
    {
        char  attack_name[20] = "";
        int   sc_dam = 0;
        int   brand  = SPWPN_NORMAL;

        int unarmed_attack = UNAT_NO_ATTACK;

        if (can_do_unarmed_combat)
        {
            if (you.species == SP_NAGA) 
                unarmed_attack = UNAT_HEADBUTT;
            else
                unarmed_attack = (coinflip() ? UNAT_HEADBUTT : UNAT_KICK);

            if ((you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
                    || player_genus(GENPC_DRACONIAN)
                    || (you.species == SP_MERFOLK && player_is_swimming())
                    || you.mutation[ MUT_STINGER ])
                && one_chance_in(3))
            {
                unarmed_attack = UNAT_TAILSLAP;
            }

            if (coinflip())
                unarmed_attack = UNAT_PUNCH;
        }

        for (unsigned char scount = 0; scount < 4; scount++)
        {
            brand = SPWPN_NORMAL;

            switch (scount)
            {
            case 0:
                if (unarmed_attack != UNAT_KICK)        //jmf: hooves mutation
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

                strcpy(attack_name, "kick");
                sc_dam = ((you.mutation[MUT_HOOVES]
                            || you.species == SP_CENTAUR) ? 10 : 5);
                break;

            case 1:
                if (unarmed_attack != UNAT_HEADBUTT)
                {
                    if ((you.species != SP_MINOTAUR
                            && (!you.mutation[MUT_HORNS]
                                && you.species != SP_KENKU))
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

                strcpy(attack_name, (you.species == SP_KENKU) ? "peck"
                                                              : "headbutt");

                sc_dam = 5 + you.mutation[MUT_HORNS] * 3;

                if (you.species == SP_MINOTAUR)
                    sc_dam += 5;

                if (you.equip[EQ_HELMET] != -1
                    && (cmp_helmet_type( you.inv[you.equip[EQ_HELMET]], THELM_HELMET )
                        || cmp_helmet_type( you.inv[you.equip[EQ_HELMET]], THELM_HELM )))
                {
                    sc_dam += 2;

                    if (cmp_helmet_desc( you.inv[you.equip[EQ_HELMET]], THELM_DESC_SPIKED )
                        || cmp_helmet_desc( you.inv[you.equip[EQ_HELMET]], THELM_DESC_HORNED ))
                    {
                        sc_dam += 3;
                    }
                }
                break;

            case 2:             /* draconians */
                if (unarmed_attack != UNAT_TAILSLAP)
                {
                    // not draconian and not wet merfolk
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

                strcpy(attack_name, "tail-slap");
                sc_dam = 6;

                if (you.mutation[ MUT_STINGER ] > 0)
                {
                    sc_dam += (you.mutation[ MUT_STINGER ] * 2 - 1);
                    brand  = SPWPN_VENOM;
                }

                /* grey dracs have spiny tails, or something */
                // maybe add this to player messaging {dlb}
                //
                // STINGER mutation doesn't give extra damage here... that
                // would probably be a bit much, we'll still get the 
                // poison bonus so it's still somewhat good.
                if (you.species == SP_GREY_DRACONIAN && you.experience_level >= 7)
                {
                    sc_dam = 12;
                }
                break;

            case 3:
                if (unarmed_attack != UNAT_PUNCH)
                    continue;

                if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SERPENT_OF_HELL
                    || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                    || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                    || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
                {
                    continue;
                }

                /* no punching with a shield or 2-handed wpn, except staves */
                if (bearing_shield || coinflip()
                    || (ur_armed && hands_reqd == HANDS_TWO_HANDED))
                {
                    continue;
                }

                strcpy(attack_name, "punch");

                /* applied twice */
                sc_dam = 5 + you.skills[SK_UNARMED_COMBAT] / 3;

                if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
                {
                    strcpy(attack_name, "slash");
                    sc_dam += 6;
                }
                break;

                /* To add more, add to while part of loop below as well */
            default:
                continue;

            }

            your_to_hit = 13 + you.dex / 2 + you.skills[SK_UNARMED_COMBAT] / 2
                                           + you.skills[SK_FIGHTING] / 5;

            if (wearing_amulet(AMU_INACCURACY))
                your_to_hit -= 5;

            make_hungry(2, true);

            if (you.hunger_state == HS_STARVING)
                your_to_hit -= 3;

            your_to_hit += slaying_bonus(PWPN_HIT);
            your_to_hit = random2(your_to_hit);

            damage = sc_dam;    //4 + you.experience_level / 3;

            alert_nearby_monsters();

            if (your_to_hit >= defender->evasion || one_chance_in(30))
            {
                bool hit = true;
                int dammod = 10;

                const int dam_stat_val = calc_stat_to_dam_base();

                if (dam_stat_val > 11)
                    dammod += random2(dam_stat_val - 11) / 3;
                if (dam_stat_val < 9)
                    dammod -= random2(9 - dam_stat_val) / 2;

                damage *= dammod;
                damage /= 10;

                damage += slaying_bonus(PWPN_DAMAGE);

                damage_done = (int) random2(damage);

                damage_done *= 40 + (random2(you.skills[SK_FIGHTING] + 1));
                damage_done /= 40;

                damage_done *= 25 + (random2(you.skills[SK_UNARMED_COMBAT]+1));
                damage_done /= 25;

                if (you.might > 1)
                    damage_done += 1 + random2(10);

                if (you.hunger_state == HS_STARVING)
                    damage_done -= random2(5);

                damage_done -= random2(1 + defender->armour_class);

                if (damage_done < 1)
                    damage_done = 0;
                else
                    hurt_monster(defender, damage_done);

                if (damage_done > 0)
                {
                    if ((!helpless || you.skills[SK_FIGHTING] < 2)
                        && one_chance_in(5))
                    {
                        exercise(SK_FIGHTING, 1);
                    }

                    if (!helpless || you.skills[SK_UNARMED_COMBAT] < 2)
                        exercise(SK_UNARMED_COMBAT, 1);

                    strcpy(info, "You ");
                    strcat(info, attack_name);
                    strcat(info, " ");
                    strcat(info, ptr_monam(defender, DESC_NOCAP_THE));

#if DEBUG_DIAGNOSTICS
                    strcat(info, " for ");
                    itoa(damage_done, st_prn, 10);
                    strcat(info, st_prn);
#endif

                    if (damage_done < HIT_WEAK)
                        strcat(info, ".");
                    else if (damage_done < HIT_MED)
                        strcat(info, "!");
                    else if (damage_done < HIT_STRONG)
                        strcat(info, "!!");
                    else
                        strcat(info, "!!!");

                    mpr(info);

                    if (brand == SPWPN_VENOM && coinflip())
                        poison_monster( defender, true );

                    if (mons_holiness(defender->type) == MH_HOLY)
                        done_good(GOOD_KILLED_ANGEL_I, 1);

                    hit = true;
                }
                else // no damage was done
                {
                    strcpy(info, "You ");
                    strcat(info, attack_name);
                    strcat(info, " ");
                    strcat(info, ptr_monam(defender, DESC_NOCAP_THE));

                    if (player_monster_visible( defender ))
                        strcat(info, ", but do no damage.");
                    else 
                        strcat(info, ".");

                    mpr(info);
                    hit = true;
                }


                if (defender->hit_points < 1)
                {
                    monster_die(defender, KILL_YOU, 0);

                    if (defender->type == MONS_GIANT_SPORE)
                    {
                        strcpy(info, "You ");
                        strcat(info, attack_name);
                        strcat(info, "the giant spore.");
                        mpr(info);
                    }
                    else if (defender->type == MONS_BALL_LIGHTNING)
                    {
                        strcpy(info, "You ");
                        strcat(info, attack_name);
                        strcat(info, "the ball lightning.");
                        mpr(info);
                    }
                    return;
                }
            }
            else
            {
                strcpy(info, "Your ");
                strcat(info, attack_name);
                strcat(info, " misses ");
                strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                strcat(info, ".");
                mpr(info);
            }
        }
    }

    if (hit)
        print_wounds(defender);

    return;
}                               // end you_attack()

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

    // This should happen after the mons_friendly check so we're 
    // only disturbed by hostiles. -- bwr
    if (you_are_delayed())
        stop_delay();

    if (attacker->type == MONS_GIANT_SPORE
        || attacker->type == MONS_BALL_LIGHTNING)
    {
        attacker->hit_points = -1;
        return;
    }

    // if a friend wants to help,  they can attack <monster_attacking>
    if (you.pet_target == MHITNOT)
        you.pet_target = monster_attacking;

    if (mons_has_ench( attacker, ENCH_SUBMERGED ))
        return;

    if (you.duration[DUR_REPEL_UNDEAD] 
        && mons_holiness( attacker->type ) == MH_UNDEAD
        && !check_mons_resist_magic( attacker, you.piety ))
    {
        simple_monster_message(attacker,
                   " tries to attack you, but is repelled by your holy aura.");
        return;
    }

    if (wearing_amulet(AMU_WARDING)
        || (you.religion == GOD_VEHUMET && you.duration[DUR_PRAYER]
            && (!player_under_penance() && you.piety >= 75)))
    {
        if (mons_has_ench(attacker, ENCH_ABJ_I, ENCH_ABJ_VI))
        {
            // should be scaled {dlb}
            if (coinflip())
            {
                simple_monster_message(attacker,
                                   " tries to attack you, but flinches away.");
                return;
            }
        }
    }

    if (grd[attacker->x][attacker->y] == DNGN_SHALLOW_WATER
        && !mons_flies( attacker )
        && !mons_flag( attacker->type, M_AMPHIBIOUS )
        && monster_habitat( attacker->type ) == DNGN_FLOOR 
        && one_chance_in(4))
    {
        simple_monster_message(attacker, " splashes around in the water.");
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
        const int con_block = 15 + attacker->hit_dice
                               + (10 * you.shield_blocks * you.shield_blocks);

        // Factors for blocking
        const int pro_block = player_shield_class() + (random2(you.dex) / 5);

        if (!you.paralysis && !you_are_delayed() && !you.conf 
            && player_monster_visible( attacker )
            && player_shield_class() > 0
            && random2(con_block) <= random2(pro_block))
        {
            you.shield_blocks++;

            snprintf( info, INFO_SIZE, "You block %s's attack.", 
                      ptr_monam( attacker, DESC_NOCAP_THE ) );

            mpr(info);

            blocked = true;
            hit = false;

            if (bearing_shield && one_chance_in(4))
                exercise(SK_SHIELDS, 1);
        }
        else if (player_light_armour() && one_chance_in(3))
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

                if (cmp_equip_race(mitm[attacker->inv[hand_used]],ISFLAG_ORCISH)
                    && mons_charclass(attacker->type) == MONS_ORC
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
            simple_monster_message(attacker, " misses you.");
        }

        if (damage_taken < 1 && hit && !blocked)
        {
            simple_monster_message(attacker,
                                    " hits you but doesn't do any damage.");
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
                && !launches_things( mitm[mmov_x].sub_type ))
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
                    const int body_arm_mass = mass_item( you.inv[you.equip[EQ_BODY_ARMOUR]] );

                    if (!player_light_armour() && coinflip()
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
                scrolls_burn(1, OBJ_SCROLLS);
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
                        mpr("You suddenly lose the ability to move!", MSGCH_WARN);

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
                strcpy(info, ptr_monam(attacker, DESC_CAP_THE));

                resistValue = player_res_cold();
                if (resistValue > 0)
                    extraDamage = 0;
                else if (resistValue == 0)
                {
                    extraDamage += roll_dice( 1, 4 );
                    strcat(info, " chills you.");
                }
                else if (resistValue < 0)
                {
                    extraDamage = roll_dice( 2, 4 );
                    strcat(info, " freezes you.");
                }
                
                mpr(info);
                damage_taken += extraDamage;
                scrolls_burn( 1, OBJ_POTIONS );
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

                scrolls_burn(1, OBJ_POTIONS);
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
            }                   // end of switch for special attacks.
            /* use brek for level drain, maybe with beam variables,
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
                itdam = mitm[attacker->inv[hand_used]].special;

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
                //if ( !one_chance_in(3) ) break;

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

                if (coinflip())
                {
                    banished(DNGN_ENTER_ABYSS);
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

        // adjust time taken if monster used weapon
        if (wpn_speed > 0)
        {
            // only get one third penalty/bonus for second weapons.
            if (runthru > 0)
                wpn_speed = (20 + wpn_speed) / 3;

            attacker->speed_increment -= (wpn_speed - 10) / 2;
        }
    }                           // end of for runthru

    return;
}                               // end monster_attack()

bool monsters_fight(int monster_attacking, int monster_attacked)
{
    struct monsters *attacker = &menv[monster_attacking];
    struct monsters *defender = &menv[monster_attacked];

    int weapon = -1;            // monster weapon, if any
    int damage_taken = 0;
    bool hit = false;
    int mmov_x = 0;
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

    if (grd[attacker->x][attacker->y] == DNGN_SHALLOW_WATER
        && !mons_flies( attacker )
        && !mons_flag( attacker->type, M_AMPHIBIOUS )
        && habitat == DNGN_FLOOR 
        && one_chance_in(4))
    {
        mpr("You hear a splashing noise.");
        return true;
    }

    if (grd[defender->x][defender->y] == DNGN_SHALLOW_WATER
        && !mons_flies(defender)
        && habitat == DNGN_DEEP_WATER)
    {
        water_attack = true;
    }

    if (mons_near(attacker) && mons_near(defender))
        sees = true;

    // now disturb defender, regardless
    behaviour_event(defender, ME_WHACK, monster_attacking);

    char runthru;

    for (runthru = 0; runthru < 4; runthru++)
    {
        char mdam = mons_damage(attacker->type, runthru);
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

        if ((attacker->type == MONS_TWO_HEADED_OGRE 
                    || attacker->type == MONS_ETTIN)
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

        if (mons_to_hit >= defender->evasion
            || ((defender->speed_increment <= 60
                 || defender->behaviour == BEH_SLEEP) && !one_chance_in(20)))
        {
            hit = true;

            if (attacker->inv[hand_used] != NON_ITEM
                && mitm[attacker->inv[hand_used]].base_type == OBJ_WEAPONS
                && !launches_things( mitm[attacker->inv[hand_used]].sub_type ))
            {
                damage_taken = random2(property( mitm[attacker->inv[hand_used]],
                                                 PWPN_DAMAGE ));

                if (cmp_equip_race(mitm[attacker->inv[hand_used]],ISFLAG_ORCISH)
                    && mons_charclass(attacker->type) == MONS_ORC
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
                strcat(info, ptr_monam(defender, DESC_NOCAP_THE));

                if (attacker->type != MONS_DANCING_WEAPON
                    && attacker->inv[hand_used] != NON_ITEM
                    && mitm[attacker->inv[hand_used]].base_type == OBJ_WEAPONS
                    && !launches_things( mitm[attacker->inv[hand_used]].sub_type ))
                {
                    strcat(info, " with ");
                    it_name(mmov_x, DESC_NOCAP_A, str_pass);       // was 7
                    strcat(info, str_pass);
                }

                strcat(info, "! ");
                mpr(info);
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
                        strcpy(info, ptr_monam(defender, DESC_NOCAP_THE));
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
                            mmov_x = attacker->inv[hand_used];

                            strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                            strcat(info, " freezes ");
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
                    switch (mons_holiness(defender->type))
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
                    if (mons_charclass(defender->type) == MONS_ORC)
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

                    if (mons_holiness(defender->type) == MH_UNDEAD
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

        // speed adjustment for weapon using monsters
        if (wpn_speed > 0)
        {
            // only get one third penalty/bonus for second weapons.
            if (runthru > 0)
                wpn_speed = (20 + wpn_speed) / 3;

            attacker->speed_increment -= (wpn_speed - 10) / 2;
        }
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


// Added by DML 6/10/99.
// For now, always returns damage: that is, it never modifies values,
// just adds 'color'.
static int weapon_type_modify( int weapnum, char noise[80], char noise2[80],
                               int damage )
{
    int weap_type = WPN_UNKNOWN;  

    if (weapnum == -1)
        weap_type = WPN_UNARMED;
    else if (item_is_staff( you.inv[weapnum] ))
        weap_type = WPN_QUARTERSTAFF;
    else if (you.inv[weapnum].base_type == OBJ_WEAPONS)
        weap_type = you.inv[weapnum].sub_type;

    noise2[0] = '\0';

    // All weak hits look the same, except for when the player 
    // has a non-weapon in hand.  -- bwr
    if (damage < HIT_WEAK)
    {
        if (weap_type != WPN_UNKNOWN)
            strcpy( noise, "hit" );
        else
            strcpy( noise, "clumsily bash" ); 

        return (damage);
    }

    // take transformations into account, if no weapon is weilded
    if (weap_type == WPN_UNARMED 
        && you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
    {
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
            case TRAN_SPIDER:
                if (damage < HIT_STRONG)
                    strcpy( noise, "bite" );
                else
                    strcpy( noise, "maul" );
                break;
            case TRAN_BLADE_HANDS:
                if (damage < HIT_MED)
                    strcpy( noise, "slash" );
                else if (damage < HIT_STRONG)
                    strcpy( noise, "slice" );
                break;
            case TRAN_ICE_BEAST:
            case TRAN_STATUE:
            case TRAN_LICH:
                if (damage < HIT_MED)
                    strcpy( noise, "punch" );
                else
                    strcpy( noise, "pummel" );
                break;
            case TRAN_DRAGON:
            case TRAN_SERPENT_OF_HELL:
                if (damage < HIT_MED)
                    strcpy( noise, "claw" );
                else if (damage < HIT_STRONG)
                    strcpy( noise, "bite" );
                else
                    strcpy( noise, "maul" );
                break;
            case TRAN_AIR:
                strcpy( noise, "buffet" );
                break;
        } // transformations

        return (damage);
    }

    switch (weap_type)
    {
    case WPN_KNIFE:
    case WPN_DAGGER:
    case WPN_SHORT_SWORD:
    case WPN_TRIDENT:
    case WPN_DEMON_TRIDENT:
    case WPN_SPEAR:
        if (damage < HIT_MED)
            strcpy( noise, "puncture" );
        else if (damage < HIT_STRONG)
            strcpy( noise, "impale" );
        else
        {
            strcpy( noise, "spit" );
            strcpy( noise2, " like a pig" );
        }
        return (damage);

    case WPN_BOW:
    case WPN_CROSSBOW:
    case WPN_HAND_CROSSBOW:
        if (damage < HIT_STRONG)
            strcpy( noise, "puncture" );
        else
            strcpy( noise, "skewer" );
        return (damage);

    case WPN_LONG_SWORD:
    case WPN_GREAT_SWORD:
    case WPN_SCIMITAR:
    case WPN_FALCHION:
    case WPN_HALBERD:
    case WPN_GLAIVE:
    case WPN_HAND_AXE:
    case WPN_WAR_AXE:
    case WPN_BROAD_AXE:
    case WPN_BATTLEAXE:
    case WPN_SCYTHE:
    case WPN_QUICK_BLADE:
    case WPN_KATANA:
    case WPN_EXECUTIONERS_AXE:
    case WPN_DOUBLE_SWORD:
    case WPN_TRIPLE_SWORD:
    case WPN_SABRE:
    case WPN_DEMON_BLADE:
        if (damage < HIT_MED)
            strcpy( noise, "slash" );
        else if (damage < HIT_STRONG)
            strcpy( noise, "slice" );
        else
        {
            strcpy( noise, "open" );
            strcpy( noise2, " like a pillowcase" );
        }
        return (damage);

    case WPN_SLING:
    case WPN_CLUB:
    case WPN_MACE:
    case WPN_FLAIL:
    case WPN_GREAT_MACE:
    case WPN_GREAT_FLAIL:
    case WPN_QUARTERSTAFF:
    case WPN_GIANT_CLUB:
    case WPN_HAMMER:
    case WPN_ANCUS:
    case WPN_MORNINGSTAR:       /*for now, just a bludgeoning weapon */
    case WPN_SPIKED_FLAIL:      /*for now, just a bludgeoning weapon */
    case WPN_EVENINGSTAR:
    case WPN_GIANT_SPIKED_CLUB:
        if (damage < HIT_MED)
            strcpy( noise, "sock" );
        else if (damage < HIT_STRONG)
            strcpy( noise, "bludgeon" );
        else
        {
            strcpy( noise, "crush" );
            strcpy( noise2, " like a grape" );
        }
        return (damage);

    case WPN_WHIP:
    case WPN_DEMON_WHIP:
        if (damage < HIT_MED)
            strcpy( noise, "whack" );
        else
            strcpy( noise, "thrash" );
        return (damage);

    case WPN_UNARMED:
        if (you.species == SP_TROLL || you.mutation[MUT_CLAWS])
        {
            if (damage < HIT_MED)
                strcpy( noise, "claw" );
            else if (damage < HIT_STRONG)
                strcpy( noise, "mangle" );
            else
                strcpy( noise, "eviscerate" );
        }
        else
        {
            if (damage < HIT_MED)
                strcpy( noise, "punch" );
            else
                strcpy( noise, "pummel" );
        }
        return (damage);

    case WPN_UNKNOWN:
    default:
        strcpy( noise, "hit" );
        return (damage);
    }
}                               // end weapon_type_modify()

// Returns a value between 0 and 10 representing the weight given to str
int weapon_str_weight( int wpn_class, int wpn_type )
{
    int  ret;

    const int  wpn_skill  = weapon_skill( wpn_class, wpn_type );
    const int  hands_reqd = hands_reqd_for_weapon( wpn_class, wpn_type );

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

    if (hands_reqd == HANDS_TWO_HANDED)
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
    const int  hands_reqd = hands_reqd_for_weapon( you.inv[weapon].base_type,
                                                   you.inv[weapon].sub_type );

    if (hands_reqd == HANDS_ONE_OR_TWO_HANDED && !shield)
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
                snprintf( info, INFO_SIZE, "You strike %s from a blind spot!",
                          ptr_monam(defender, DESC_NOCAP_THE) );
            }
            else
            {
                snprintf( info, INFO_SIZE, "You catch %s momentarily off-guard.",
                          ptr_monam(defender, DESC_NOCAP_THE) );
            }
            break;
        case 2:     // confused/fleeing
            if (r<4)
            {
                snprintf( info, INFO_SIZE, "You catch %s completely off-guard!",
                           ptr_monam(defender, DESC_NOCAP_THE) );
            }
            else
            {
                snprintf( info, INFO_SIZE, "You strike %s from behind!",
                          ptr_monam(defender, DESC_NOCAP_THE) );
            }
            break;
        case 1:
            snprintf( info, INFO_SIZE, "%s fails to defend %s.", 
                      ptr_monam(defender, DESC_CAP_THE), 
                      mons_pronoun( defender->type, PRONOUN_REFLEXIVE ) );
            break;
    } // end switch

    mpr(info);
}
            
