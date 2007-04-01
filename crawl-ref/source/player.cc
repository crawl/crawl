/*
 *  File:       player.cc
 *  Summary:    Player related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <6> 7/30/99 BWR   Added player_spell_levels()
 * <5> 7/13/99 BWR   Added player_res_electricity()
 *                   and player_hunger_rate()
 * <4> 6/22/99 BWR   Racial adjustments to stealth and Armour.
 * <3> 5/20/99 BWR   Fixed problems with random stat increases, added kobold
 *                   stat increase.  increased EV recovery for Armour.
 * <2> 5/08/99 LRH   display_char_status correctly handles magic_contamination.
 * <1> -/--/-- LRH   Created
 */

#include "AppHdr.h"
#include "player.h"

#ifdef DOS
#include <conio.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "externs.h"

#include "clua.h"
#include "delay.h"
#include "effects.h"
#include "fight.h"
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spells1.h"
#include "spells3.h"
#include "spl-util.h"
#include "spells4.h"
#include "stuff.h"
#include "transfor.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"

std::string pronoun_you(description_level_type desc)
{
    switch (desc)
    {
    case DESC_CAP_A: case DESC_CAP_THE:
        return "You";
    case DESC_NOCAP_A: case DESC_NOCAP_THE:
    default:
        return "you";
    case DESC_CAP_YOUR:
        return "Your";
    case DESC_NOCAP_YOUR:
        return "your";
    }
}

//////////////////////////////////////////////////////////////////////////
/*
   you.duration []: //jmf: obsolete, see enum.h instead
                    //[ds] Well, can we lose it yet?
   0 - liquid flames
   1 - icy armour
   2 - repel missiles
   3 - prayer
   4 - regeneration
   5 - vorpal blade
   6 - fire brand
   7 - ice brand
   8 - lethal infusion
   9 - swiftness
   10 - insulation
   11 - stonemail
   12 - controlled flight
   13 - teleport
   14 - control teleport
   15 - poison weapon
   16 - resist poison
   17 - breathe something
   18 - transformation (duration)
   19 - death channel
   20 - deflect missiles
 */

/* attributes
   0 - resist lightning
   1 - spec_air
   2 - spec_earth
   3 - control teleport
   4 - walk slowly (eg naga)
   5 - transformation (form)
   6 - Nemelex card gift countdown
   7 - Nemelex has given you a card table
   8 - How many demonic powers a dspawn has
 */

/* armour list
   0 - wielded
   1 - cloak
   2 - helmet
   3 - gloves
   4 - boots
   5 - shield
   6 - body armour
   7 - ring 0
   8 - ring 1
   9 - amulet
 */

/* Contains functions which return various player state vars,
   and other stuff related to the player. */

int species_exp_mod(char species);
void ability_increase(void);

// Use this function whenever the player enters (or lands and thus re-enters)
// a grid.  
//
// stepped - normal walking moves
// allow_shift - allowed to scramble in any direction out of lava/water
// force - ignore safety checks, move must happen (traps, lava/water).
bool move_player_to_grid( int x, int y, bool stepped, bool allow_shift,
                          bool force )
{
    ASSERT( in_bounds( x, y ) );

    int id;
    // assuming that entering the same square means coming from above (levitate)
    const bool from_above = (you.x_pos == x && you.y_pos == y);
    const int old_grid = (from_above) ? static_cast<int>(DNGN_FLOOR) 
                                      : grd[you.x_pos][you.y_pos];
    const int new_grid = grd[x][y];

    // really must be clear
    ASSERT( !grid_is_solid( new_grid ) );

    // if (grid_is_solid( new_grid ))
    //     return (false);

    // better not be an unsubmerged monster either:
    ASSERT( mgrd[x][y] == NON_MONSTER 
            || mons_is_submerged( &menv[ mgrd[x][y] ] ));

    // if (mgrd[x][y] != NON_MONSTER && !mons_is_submerged( &menv[ mgrd[x][y] ] ))
    //     return (false);

    // if we're walking along, give a chance to avoid trap
    if (stepped 
        && !force
        && new_grid == DNGN_UNDISCOVERED_TRAP)
    {
        const int skill = 4 + you.skills[SK_TRAPS_DOORS] 
                            + you.mutation[MUT_ACUTE_VISION]
                            - 2 * you.mutation[MUT_BLURRY_VISION];

        if (random2( skill ) > 6)
        {
            mprf( MSGCH_WARN,
                  "Wait a moment, %s!  Do you really want to step there?",
                  you.your_name );

            more();

            exercise( SK_TRAPS_DOORS, 3 );

            you.turn_is_over = false;

            id = trap_at_xy( x, y );
            if (id != -1)
                grd[x][y] = trap_category( env.trap[id].type );

            return (false);
        }
    }

    // only consider terrain if player is not levitating
    if (!player_is_levitating())
    {
        // XXX: at some point we're going to need to fix the swimming 
        // code to handle burden states.
        if (is_grid_dangerous(new_grid))
        {
            // lava and dangerous deep water (ie not merfolk)
            int entry_x = (stepped) ? you.x_pos : x;
            int entry_y = (stepped) ? you.y_pos : y;

            if (stepped && !force && !you.conf)
            {
                bool okay = yesno( "Do you really want to step there?", 
                                   false, 'n' );

                if (!okay)
                {   
                    canned_msg(MSG_OK);
                    return (false);
                }
            }

            // have to move now so fall_into_a_pool will work
            you.x_pos = x;
            you.y_pos = y;

            viewwindow( true, false );

            // if true, we were shifted and so we're done.
            if (fall_into_a_pool( entry_x, entry_y, allow_shift, new_grid ))
                return (true);
        }
        else if (new_grid == DNGN_SHALLOW_WATER || new_grid == DNGN_DEEP_WATER)
        {
            // safer water effects
            if (you.species == SP_MERFOLK)
            {
                if (old_grid != DNGN_SHALLOW_WATER 
                    && old_grid != DNGN_DEEP_WATER)
                {
                    if (stepped)
                        mpr("Your legs become a tail as you enter the water.");
                    else
                        mpr("Your legs become a tail as you dive into the water.");

                    merfolk_start_swimming();
                }
            }
            else 
            {
                ASSERT( new_grid != DNGN_DEEP_WATER );

                if (!stepped)
                    noisy(SL_SPLASH, you.x_pos, you.y_pos, "Splash!");

                you.time_taken *= 13 + random2(8);
                you.time_taken /= 10;

                if (old_grid != DNGN_SHALLOW_WATER)
                {
                    mprf( "You %s the shallow water.",
                          (stepped ? "enter" : "fall into") );

                    mpr("Moving in this stuff is going to be slow.");

                    if (you.invis)
                        mpr( "... and don't expect to remain undetected." );
                }
            }
        }
    }

    // move the player to location
    you.x_pos = x;
    you.y_pos = y;

    viewwindow( true, false );

    // Other Effects:
    // clouds -- do we need this? (always seems to double up with acr.cc call)
    // if (is_cloud( you.x_pos, you.y_pos ))
    //     in_a_cloud();

    // icy shield goes down over lava
    if (new_grid == DNGN_LAVA)
        expose_player_to_element( BEAM_LAVA );

    // traps go off:
    if (new_grid >= DNGN_TRAP_MECHANICAL && new_grid <= DNGN_UNDISCOVERED_TRAP)
    {
        id = trap_at_xy( you.x_pos, you.y_pos );

        if (id != -1)
        {
            bool trap_known = true;

            if (new_grid == DNGN_UNDISCOVERED_TRAP)
            {
                trap_known = false;

                const int type = trap_category( env.trap[id].type );

                grd[you.x_pos][you.y_pos] = type;
                set_envmap_char(you.x_pos, you.y_pos, get_sightmap_char(type));
            }

            // not easy to blink onto a trap without setting it off:
            if (!stepped)
                trap_known = false;

            if (!player_is_levitating()
                || trap_category( env.trap[id].type ) != DNGN_TRAP_MECHANICAL)
            {
                handle_traps(env.trap[id].type, id, trap_known);
            }
        }
    }

    return (true);
}

bool player_can_swim()
{
    return you.can_swim();
}

bool is_grid_dangerous(int grid)
{
    return (!player_is_levitating()
            && (grid == DNGN_LAVA
                || (grid == DNGN_DEEP_WATER && !player_can_swim())));
}

bool player_in_mappable_area( void )
{
    return (you.level_type != LEVEL_LABYRINTH && you.level_type != LEVEL_ABYSS);
}

bool player_in_branch( int branch )
{
    return (you.level_type == LEVEL_DUNGEON && you.where_are_you == branch);
}

bool player_in_hell( void )
{
    return (you.level_type == LEVEL_DUNGEON
            && (you.where_are_you >= BRANCH_DIS 
                && you.where_are_you <= BRANCH_THE_PIT)
            && you.where_are_you != BRANCH_VESTIBULE_OF_HELL);
}

bool player_in_water(void)
{
    return (!player_is_levitating()
            && grid_is_water(grd[you.x_pos][you.y_pos]));
}

bool player_is_swimming(void)
{
    return you.swimming();
}

bool player_floundering()
{
    return (player_in_water() && !player_can_swim());
}

bool player_under_penance(void)
{
    if (you.religion != GOD_NO_GOD)
        return (you.penance[you.religion]);
    else
        return (false);
}

bool player_genus(unsigned char which_genus, unsigned char species)
{
    if (species == SP_UNKNOWN)
        species = you.species;

    switch (species)
    {
    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_GOLDEN_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_UNK0_DRACONIAN:
    case SP_UNK1_DRACONIAN:
    case SP_BASE_DRACONIAN:
        return (which_genus == GENPC_DRACONIAN);

    case SP_ELF:
    case SP_HIGH_ELF:
    case SP_GREY_ELF:
    case SP_DEEP_ELF:
    case SP_SLUDGE_ELF:
        return (which_genus == GENPC_ELVEN);

    case SP_HILL_DWARF:
    case SP_MOUNTAIN_DWARF:
        return (which_genus == GENPC_DWARVEN);

    default:
        break;
    }

    return (false);
}                               // end player_genus()

// Returns the item in the given equipment slot, NULL if the slot is empty.
// eq must be in [EQ_WEAPON, EQ_AMULET], or bad things will happen.
item_def *player_slot_item(equipment_type eq)
{
    return you.slot_item(eq);
}

// Returns the item in the player's weapon slot.
item_def *player_weapon()
{
    return you.weapon();
}

bool player_weapon_wielded()
{
    const int wpn = you.equip[EQ_WEAPON];

    if (wpn == -1)
        return (false);

    if (you.inv[wpn].base_type != OBJ_WEAPONS 
        && you.inv[wpn].base_type != OBJ_STAVES)
    {
        return (false);
    }

    // FIXME: This needs to go in eventually. 
    /*
    // should never have a bad "shape" weapon in hand
    ASSERT( check_weapon_shape( you.inv[wpn], false ) );

    if (!check_weapon_wieldable_size( you.inv[wpn], player_size() ))
        return (false);
     */

    return (true);
}

// Returns the you.inv[] index of our wielded weapon or -1 (no item, not wield)
int get_player_wielded_item()
{
    return (you.equip[EQ_WEAPON]);
}

int get_player_wielded_weapon()
{
    return (player_weapon_wielded()? get_player_wielded_item() : -1);
}

// Looks in equipment "slot" to see if there is an equiped "sub_type".
// Returns number of matches (in the case of rings, both are checked)
int player_equip( int slot, int sub_type, bool calc_unid )
{
    int ret = 0;

    switch (slot)
    {
    case EQ_WEAPON:
        // Hands can have more than just weapons.
        if (you.equip[EQ_WEAPON] != -1
            && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_WEAPONS
            && you.inv[you.equip[EQ_WEAPON]].sub_type == sub_type)
        {
            ret++;
        }
        break;

    case EQ_STAFF:
        // Like above, but must be magical stave.
        if (you.equip[EQ_WEAPON] != -1
            && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_STAVES
            && you.inv[you.equip[EQ_WEAPON]].sub_type == sub_type
            && (calc_unid ||
                item_type_known(you.inv[you.equip[EQ_WEAPON]])))
        {
            ret++;
        }
        break;

    case EQ_RINGS:
        if (you.equip[EQ_LEFT_RING] != -1 
            && you.inv[you.equip[EQ_LEFT_RING]].sub_type == sub_type
            && (calc_unid || 
                item_type_known(you.inv[you.equip[EQ_LEFT_RING]])))
        {
            ret++;
        }

        if (you.equip[EQ_RIGHT_RING] != -1 
            && you.inv[you.equip[EQ_RIGHT_RING]].sub_type == sub_type
            && (calc_unid || 
               item_type_known(you.inv[you.equip[EQ_RIGHT_RING]])))
        {
            ret++;
        }
        break;

    case EQ_RINGS_PLUS:
        if (you.equip[EQ_LEFT_RING] != -1 
            && you.inv[you.equip[EQ_LEFT_RING]].sub_type == sub_type)
        {
            ret += you.inv[you.equip[EQ_LEFT_RING]].plus;
        }

        if (you.equip[EQ_RIGHT_RING] != -1 
            && you.inv[you.equip[EQ_RIGHT_RING]].sub_type == sub_type)
        {
            ret += you.inv[you.equip[EQ_RIGHT_RING]].plus;
        }
        break;

    case EQ_RINGS_PLUS2:
        if (you.equip[EQ_LEFT_RING] != -1 
            && you.inv[you.equip[EQ_LEFT_RING]].sub_type == sub_type)
        {
            ret += you.inv[you.equip[EQ_LEFT_RING]].plus2;
        }

        if (you.equip[EQ_RIGHT_RING] != -1 
            && you.inv[you.equip[EQ_RIGHT_RING]].sub_type == sub_type)
        {
            ret += you.inv[you.equip[EQ_RIGHT_RING]].plus2;
        }
        break;

    case EQ_ALL_ARMOUR:
        // doesn't make much sense here... be specific. -- bwr
        break;

    default:
        if (you.equip[slot] != -1 
            && you.inv[you.equip[slot]].sub_type == sub_type
            && (calc_unid || 
                item_type_known(you.inv[you.equip[slot]])))
        {
            ret++;
        }
        break;
    }

    return (ret);
}


// Looks in equipment "slot" to see if equiped item has "special" ego-type
// Returns number of matches (jewellery returns zero -- no ego type).
// [ds] There's no equivalent of calc_unid or req_id because as of now, weapons
// and armour type-id on wield/wear.
int player_equip_ego_type( int slot, int special )
{
    int ret = 0;
    int wpn;

    switch (slot)
    {
    case EQ_WEAPON:
        // This actually checks against the "branding", so it will catch
        // randart brands, but not fixed artefacts.  -- bwr

        // Hands can have more than just weapons.
        wpn = you.equip[EQ_WEAPON];
        if (wpn != -1
            && you.inv[wpn].base_type == OBJ_WEAPONS
            && get_weapon_brand( you.inv[wpn] ) == special)
        {
            ret++;
        }
        break;

    case EQ_LEFT_RING:
    case EQ_RIGHT_RING:
    case EQ_AMULET:
    case EQ_STAFF:
    case EQ_RINGS:
    case EQ_RINGS_PLUS:
    case EQ_RINGS_PLUS2:
        // no ego types for these slots
        break;

    case EQ_ALL_ARMOUR:
        // Check all armour slots:
        for (int i = EQ_CLOAK; i <= EQ_BODY_ARMOUR; i++)
        {
            if (you.equip[i] != -1 
                && get_armour_ego_type( you.inv[you.equip[i]] ) == special)
            {
                ret++;
            }
        }
        break;

    default:
        // Check a specific armour slot for an ego type:
        if (you.equip[slot] != -1
            && get_armour_ego_type( you.inv[you.equip[slot]] ) == special)
        {
            ret++;
        }
        break;
    }

    return (ret);
}

int player_damage_type( void )
{
    return you.damage_type();
}

// returns band of player's melee damage
int player_damage_brand( void )
{
    return you.damage_brand();
}

int player_teleport(bool calc_unid)
{
    int tp = 0;

    /* rings */
    tp += 8 * player_equip( EQ_RINGS, RING_TELEPORTATION, calc_unid );

    /* mutations */
    tp += you.mutation[MUT_TELEPORT] * 3;

    /* randart weapons only */
    if (you.equip[EQ_WEAPON] != -1
        && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_WEAPONS
        && is_random_artefact( you.inv[you.equip[EQ_WEAPON]] ))
    {
        tp += scan_randarts(RAP_CAUSE_TELEPORTATION, calc_unid);
    }

    return tp;
}                               // end player_teleport()

int player_regen(void)
{
    int rr = you.hp_max / 3;

    if (rr > 20)
        rr = 20 + ((rr - 20) / 2);

    /* rings */
    rr += 40 * player_equip( EQ_RINGS, RING_REGENERATION );

    /* spell */
    if (you.duration[DUR_REGENERATION])
        rr += 100;

    /* troll leather (except for trolls) */
    if (player_equip( EQ_BODY_ARMOUR, ARM_TROLL_LEATHER_ARMOUR ) &&
        you.species != SP_TROLL)
        rr += 30;

    /* fast heal mutation */
    rr += you.mutation[MUT_REGENERATION] * 20;

    /* ghouls heal slowly */
    // dematerialized people heal slowly
    // dematerialized ghouls shouldn't heal any more slowly -- bwr
    if ((you.species == SP_GHOUL
            && (you.attribute[ATTR_TRANSFORMATION] == TRAN_NONE
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS))
        || you.attribute[ATTR_TRANSFORMATION] == TRAN_AIR)
    {
        rr /= 2;
    }

    if (rr < 1)
        rr = 1;

    return (rr);
}

int player_hunger_rate(void)
{
    int hunger = 3;

    // jmf: hunger isn't fair while you can't eat
    // Actually, it is since you can detransform any time you like -- bwr
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_AIR)
        return 0;

    if ( you.species == SP_TROLL )
        hunger += 3;            // in addition to the +3 for fast metabolism

    if (you.duration[DUR_REGENERATION] > 0)
        hunger += 4;

    // moved here from acr.cc... maintaining the >= 40 behaviour
    if (you.hunger >= 40)
    {
        if (you.invis > 0)
            hunger += 5;

        // berserk has its own food penalty -- excluding berserk haste
        if (you.haste > 0 && !you.berserker)
            hunger += 5;
    }

    hunger += you.mutation[MUT_FAST_METABOLISM];

    if (you.mutation[MUT_SLOW_METABOLISM] > 2)
        hunger -= 2;
    else if (you.mutation[MUT_SLOW_METABOLISM] > 0)
        hunger--;

    // rings
    hunger += 2 * player_equip( EQ_RINGS, RING_REGENERATION );
    hunger += 4 * player_equip( EQ_RINGS, RING_HUNGER );
    hunger -= 2 * player_equip( EQ_RINGS, RING_SUSTENANCE );

    // weapon ego types
    hunger += 6 * player_equip_ego_type( EQ_WEAPON, SPWPN_VAMPIRICISM ); 
    hunger += 9 * player_equip_ego_type( EQ_WEAPON, SPWPN_VAMPIRES_TOOTH ); 

    // troll leather armour 
    hunger += player_equip( EQ_BODY_ARMOUR, ARM_TROLL_LEATHER_ARMOUR );

    // randarts
    hunger += scan_randarts(RAP_METABOLISM);

    // burden
    hunger += you.burden_state;

    if (hunger < 1)
        hunger = 1;

    return (hunger);
}

int player_spell_levels(void)
{
    int sl = (you.experience_level - 1) + (you.skills[SK_SPELLCASTING] * 2);

    bool fireball = false;
    bool delayed_fireball = false;

    if (sl > 99)
        sl = 99;

    for (int i = 0; i < 25; i++)
    {
        if (you.spells[i] == SPELL_FIREBALL)
            fireball = true;
        else if (you.spells[i] == SPELL_DELAYED_FIREBALL)
            delayed_fireball = true;

        if (you.spells[i] != SPELL_NO_SPELL)
            sl -= spell_difficulty(you.spells[i]);
    }

    // Fireball is free for characters with delayed fireball
    if (fireball && delayed_fireball)
        sl += spell_difficulty( SPELL_FIREBALL );

    // Note: This can happen because of level drain.  Maybe we should
    // force random spells out when that happens. -- bwr
    if (sl < 0)
        sl = 0;

    return (sl);
}

int player_res_magic(void)
{
    int rm = 0;

    switch (you.species)
    {
    default:
        rm = you.experience_level * 3;
        break;
    case SP_HIGH_ELF:
    case SP_GREY_ELF:
    case SP_ELF:
    case SP_SLUDGE_ELF:
    case SP_HILL_DWARF:
    case SP_MOUNTAIN_DWARF:
        rm = you.experience_level * 4;
        break;
    case SP_NAGA:
        rm = you.experience_level * 5;
        break;
    case SP_PURPLE_DRACONIAN:
    case SP_GNOME:
    case SP_DEEP_ELF:
        rm = you.experience_level * 6;
        break;
    case SP_SPRIGGAN:
        rm = you.experience_level * 7;
        break;
    }

    /* armour  */
    rm += 30 * player_equip_ego_type( EQ_ALL_ARMOUR, SPARM_MAGIC_RESISTANCE );

    /* rings of magic resistance */
    rm += 40 * player_equip( EQ_RINGS, RING_PROTECTION_FROM_MAGIC );

    /* randarts */
    rm += scan_randarts(RAP_MAGIC);

    /* Enchantment skill */
    rm += 2 * you.skills[SK_ENCHANTMENTS];

    /* Mutations */
    rm += 30 * you.mutation[MUT_MAGIC_RESISTANCE];

    /* transformations */
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH)
        rm += 50;

    return rm;
}

int player_res_steam(bool calc_unid)
{
    int res = 0;

    if (you.species == SP_PALE_DRACONIAN && you.experience_level > 5)
        res += 2;

    if (player_equip(EQ_BODY_ARMOUR, ARM_STEAM_DRAGON_ARMOUR))
        res += 2;

    return (res + player_res_fire(calc_unid) / 2);
}

bool player_can_smell()
{
    return (you.species != SP_MUMMY);
}

int player_res_fire(bool calc_unid)
{
    int rf = 0;

    /* rings of fire resistance/fire */
    rf += player_equip( EQ_RINGS, RING_PROTECTION_FROM_FIRE, calc_unid );
    rf += player_equip( EQ_RINGS, RING_FIRE, calc_unid );

    /* rings of ice */
    rf -= player_equip( EQ_RINGS, RING_ICE, calc_unid );

    /* Staves */
    rf += player_equip( EQ_STAFF, STAFF_FIRE, calc_unid );

    // body armour:
    rf += 2 * player_equip( EQ_BODY_ARMOUR, ARM_DRAGON_ARMOUR );
    rf += player_equip( EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_ARMOUR );
    rf -= player_equip( EQ_BODY_ARMOUR, ARM_ICE_DRAGON_ARMOUR );

    // ego armours
    rf += player_equip_ego_type( EQ_ALL_ARMOUR, SPARM_FIRE_RESISTANCE );
    rf += player_equip_ego_type( EQ_ALL_ARMOUR, SPARM_RESISTANCE );

    // randart weapons:
    rf += scan_randarts(RAP_FIRE, calc_unid);

    // species:
    if (you.species == SP_MUMMY)
        rf--;

    // mutations:
    rf += you.mutation[MUT_HEAT_RESISTANCE];

    if (you.fire_shield)
        rf += 2;

    // transformations:
    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_ICE_BEAST:
        rf--;
        break;
    case TRAN_DRAGON:
        rf += 2;
        break;
    case TRAN_SERPENT_OF_HELL:
        rf += 2;
        break;
    case TRAN_AIR:
        rf -= 2;
        break;
    }

    if (rf < -3)
        rf = -3;
    else if (rf > 3)
        rf = 3;

    return (rf);
}

int player_res_cold(bool calc_unid)
{
    int rc = 0;

    /* rings of fire resistance/fire */
    rc += player_equip( EQ_RINGS, RING_PROTECTION_FROM_COLD, calc_unid );
    rc += player_equip( EQ_RINGS, RING_ICE, calc_unid );

    /* rings of ice */
    rc -= player_equip( EQ_RINGS, RING_FIRE, calc_unid );

    /* Staves */
    rc += player_equip( EQ_STAFF, STAFF_COLD, calc_unid );

    // body armour:
    rc += 2 * player_equip( EQ_BODY_ARMOUR, ARM_ICE_DRAGON_ARMOUR );
    rc += player_equip( EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_ARMOUR );
    rc -= player_equip( EQ_BODY_ARMOUR, ARM_DRAGON_ARMOUR );

    // ego armours
    rc += player_equip_ego_type( EQ_ALL_ARMOUR, SPARM_COLD_RESISTANCE );
    rc += player_equip_ego_type( EQ_ALL_ARMOUR, SPARM_RESISTANCE );

    // randart weapons:
    rc += scan_randarts(RAP_COLD, calc_unid);

    // mutations:
    rc += you.mutation[MUT_COLD_RESISTANCE];

    if (you.fire_shield)
        rc -= 2;

    // transformations:
    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_ICE_BEAST:
        rc += 3;
        break;
    case TRAN_DRAGON:
        rc--;
        break;
    case TRAN_LICH:
        rc++;
        break;
    case TRAN_AIR:
        rc -= 2;
        break;
    }

    if (rc < -3)
        rc = -3;
    else if (rc > 3)
        rc = 3;

    return (rc);
}

int player_res_acid(bool consider_unidentified_gear)
{
    int res = 0;
    if (!transform_changed_physiology())
    {
        if (you.species == SP_GOLDEN_DRACONIAN 
                    && you.experience_level >= 7)
            res += 2;

        res += you.mutation[MUT_YELLOW_SCALES] * 2 / 3;
    }

    if (wearing_amulet(AMU_RESIST_CORROSION, consider_unidentified_gear))
        res++;

    return (res);
}

// Returns a factor X such that post-resistance acid damage can be calculated
// as pre_resist_damage * X / 100.
int player_acid_resist_factor()
{
    int res = player_res_acid();

    int factor = 100;

    if (res == 1)
        factor = 50;
    else if (res == 2)
        factor = 34;
    else if (res > 2)
    {
        factor = 30;
        while (res-- > 2 && factor >= 20)
            factor = factor * 90 / 100;
    }

    return (factor);
}

int player_res_electricity(bool calc_unid)
{
    int re = 0;

    if (you.duration[DUR_INSULATION])
        re++;

    // staff
    re += player_equip( EQ_STAFF, STAFF_AIR, calc_unid );

    // body armour:
    re += player_equip( EQ_BODY_ARMOUR, ARM_STORM_DRAGON_ARMOUR );

    // randart weapons:
    re += scan_randarts(RAP_ELECTRICITY, calc_unid);

    // species:
    if (you.species == SP_BLACK_DRACONIAN && you.experience_level > 17)
        re++;

    // mutations:
    if (you.mutation[MUT_SHOCK_RESISTANCE])
        re++;

    // transformations:
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_STATUE)
        re += 1;

    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_AIR)
        re += 2;  // mutliple levels currently meaningless

    if (you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION] > 0)
        re = 3;
    else if (re > 1)
        re = 1;

    return (re);
}                               // end player_res_electricity()

// Does the player resist asphyxiation?
bool player_res_asphyx()
{
    // The undead are immune to asphyxiation, or so we'll assume.
    if (you.is_undead)
        return (true);

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_LICH:
    case TRAN_STATUE:
    case TRAN_SERPENT_OF_HELL:
    case TRAN_AIR:
        return (true);
    }
    return (false);
}

bool player_control_teleport(bool calc_unid) {
    return ( you.duration[DUR_CONTROL_TELEPORT] ||
             player_equip(EQ_RINGS, RING_TELEPORT_CONTROL, calc_unid) ||
             you.mutation[MUT_TELEPORT_CONTROL] );
}

int player_res_torment(bool)
{
    return (you.is_undead || you.mutation[MUT_TORMENT_RESISTANCE]);
}

// funny that no races are susceptible to poisons {dlb}
int player_res_poison(bool calc_unid)
{
    int rp = 0;

    /* rings of poison resistance */
    rp += player_equip( EQ_RINGS, RING_POISON_RESISTANCE, calc_unid );

    /* Staves */
    rp += player_equip( EQ_STAFF, STAFF_POISON, calc_unid );

    /* the staff of Olgreb: */
    if (you.equip[EQ_WEAPON] != -1
        && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_WEAPONS
        && you.inv[you.equip[EQ_WEAPON]].special == SPWPN_STAFF_OF_OLGREB)
    {
        rp++;
    }

    // ego armour:
    rp += player_equip_ego_type( EQ_ALL_ARMOUR, SPARM_POISON_RESISTANCE );

    // body armour:
    rp += player_equip( EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_ARMOUR );
    rp += player_equip( EQ_BODY_ARMOUR, ARM_SWAMP_DRAGON_ARMOUR );

    // spells:
    if (you.duration[DUR_RESIST_POISON] > 0)
        rp++;

    // randart weapons:
    rp += scan_randarts(RAP_POISON, calc_unid);

    // mutations:
    rp += you.mutation[MUT_POISON_RESISTANCE];

    // transformations:
    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_LICH:
    case TRAN_ICE_BEAST:
    case TRAN_STATUE:
    case TRAN_DRAGON:
    case TRAN_SERPENT_OF_HELL:
    case TRAN_AIR:
        rp++;
        break;
    }

    if (rp > 1)
        rp = 1;

    return (rp);
}                               // end player_res_poison()

int player_spec_death()
{
    int sd = 0;

    /* Staves */
    sd += player_equip( EQ_STAFF, STAFF_DEATH );

    // body armour:
    if (player_equip_ego_type( EQ_BODY_ARMOUR, SPARM_ARCHMAGI ))
        sd++;

    // species:
    if (you.species == SP_MUMMY)
    {
        if (you.experience_level >= 13)
            sd++;
        if (you.experience_level >= 26)
            sd++;
    }

    // transformations:
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH)
        sd++;

    return sd;
}

int player_spec_holy()
{
    //if ( you.char_class == JOB_PRIEST || you.char_class == JOB_PALADIN )
    //  return 1;
    return 0;
}

int player_spec_fire()
{
    int sf = 0;

    // staves:
    sf += player_equip( EQ_STAFF, STAFF_FIRE );

    // rings of fire:
    sf += player_equip( EQ_RINGS, RING_FIRE );

    if (you.fire_shield)
        sf++;

    return sf;
}

int player_spec_cold()
{
    int sc = 0;

    // staves:
    sc += player_equip( EQ_STAFF, STAFF_COLD );

    // rings of ice:
    sc += player_equip( EQ_RINGS, RING_ICE );

    return sc;
}

int player_spec_earth()
{
    int se = 0;

    /* Staves */
    se += player_equip( EQ_STAFF, STAFF_EARTH );

    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_AIR)
        se--;

    return se;
}

int player_spec_air()
{
    int sa = 0;

    /* Staves */
    sa += player_equip( EQ_STAFF, STAFF_AIR );

    //jmf: this was too good
    //if (you.attribute[ATTR_TRANSFORMATION] == TRAN_AIR)
    //  sa++;
    return sa;
}

int player_spec_conj()
{
    int sc = 0;

    /* Staves */
    sc += player_equip( EQ_STAFF, STAFF_CONJURATION );

    // armour of the Archmagi 
    if (player_equip_ego_type( EQ_BODY_ARMOUR, SPARM_ARCHMAGI ))
        sc++;

    return sc;
}

int player_spec_ench()
{
    int se = 0;

    /* Staves */
    se += player_equip( EQ_STAFF, STAFF_ENCHANTMENT );

    // armour of the Archmagi
    if (player_equip_ego_type( EQ_BODY_ARMOUR, SPARM_ARCHMAGI ))
        se++;

    return se;
}

int player_spec_summ()
{
    int ss = 0;

    /* Staves */
    ss += player_equip( EQ_STAFF, STAFF_SUMMONING );

    // armour of the Archmagi
    if (player_equip_ego_type( EQ_BODY_ARMOUR, SPARM_ARCHMAGI ))
        ss++;

    return ss;
}

int player_spec_poison()
{
    int sp = 0;

    /* Staves */
    sp += player_equip( EQ_STAFF, STAFF_POISON );

    if (you.equip[EQ_WEAPON] != -1
        && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_WEAPONS
        && you.inv[you.equip[EQ_WEAPON]].special == SPWPN_STAFF_OF_OLGREB)
    {
        sp++;
    }

    return sp;
}

int player_energy()
{
    int pe = 0;

    // Staves
    pe += player_equip( EQ_STAFF, STAFF_ENERGY );

    return pe;
}

int player_prot_life(bool calc_unid)
{
    int pl = 0;

    if (wearing_amulet(AMU_WARDING, calc_unid))
        ++pl;

    // rings
    pl += player_equip( EQ_RINGS, RING_LIFE_PROTECTION, calc_unid );

    // armour: (checks body armour only)
    pl += player_equip_ego_type( EQ_ALL_ARMOUR, SPARM_POSITIVE_ENERGY );

    if (you.is_undead)
        pl += 3;

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_STATUE:
        pl += 1;
        break;

    case TRAN_SERPENT_OF_HELL:
        pl += 2;
        break;

    case TRAN_LICH:
        pl += 3;
        break;

    default:
        break;
    }

    // randart wpns
    pl += scan_randarts(RAP_NEGATIVE_ENERGY, calc_unid);

    // demonic power
    pl += you.mutation[MUT_NEGATIVE_ENERGY_RESISTANCE];

    if (pl > 3)
        pl = 3;

    return (pl);
}

// Returns the movement speed for a player ghost. Note that this is a real
// speed, not a movement cost, so higher is better.
int player_ghost_base_movement_speed()
{
    int speed = you.species == SP_NAGA? 8 : 10;

    if (you.mutation[MUT_FAST])
        speed += you.mutation[MUT_FAST] + 1;

    if (player_equip_ego_type( EQ_BOOTS, SPARM_RUNNING ))
        speed += 2;

    // Cap speeds.
    if (speed < 6)
        speed = 6;
    
    if (speed > 13)
        speed = 13;

    return (speed);
}

// New player movement speed system... allows for a bit more that
// "player runs fast" and "player walks slow" in that the speed is
// actually calculated (allowing for centaurs to get a bonus from
// swiftness and other such things).  Levels of the mutation now
// also have meaning (before they all just meant fast).  Most of
// this isn't as fast as it used to be (6 for having anything), but
// even a slight speed advantage is very good... and we certainly don't
// want to go past 6 (see below). -- bwr
int player_movement_speed(void)
{
    int mv = 10;

    if (player_is_swimming())
    {
        // This is swimming... so it doesn't make sense to really
        // apply the other things (the mutation is "cover ground",
        // swiftness is an air spell, can't wear boots, can't be
        // transformed).
        mv = 6;
    }
    else
    {
        /* transformations */
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER)
            mv = 8;

        /* armour */
        if (player_equip_ego_type( EQ_BOOTS, SPARM_RUNNING ))
            mv -= 2;

        if (player_equip_ego_type( EQ_BODY_ARMOUR, SPARM_PONDEROUSNESS ))
            mv += 2;

        // Swiftness is an Air spell, it doesn't work in water...
        // but levitating players will move faster. -- bwr
        if (you.duration[DUR_SWIFTNESS] > 0 && !player_in_water())
            mv -= (player_is_levitating() ? 4 : 2);

        /* Mutations: -2, -3, -4, unless innate and shapechanged */
        if (you.mutation[MUT_FAST] > 0 &&
            (!you.demon_pow[MUT_FAST] || !player_is_shapechanged()))
            mv -= (you.mutation[MUT_FAST] + 1);

        // Burden
        if (you.burden_state == BS_ENCUMBERED)
            mv += 1;
        else if (you.burden_state == BS_OVERLOADED)
            mv += 3;
    }

    // We'll use the old value of six as a minimum, with haste this could
    // end up as a speed of three, which is about as fast as we want
    // the player to be able to go (since 3 is 3.33x as fast and 2 is 5x,
    // which is a bit of a jump, and a bit too fast) -- bwr
    if (mv < 6)
        mv = 6;

    // Nagas move slowly:
    if (you.species == SP_NAGA && !player_is_shapechanged())
    {
        mv *= 14;
        mv /= 10;
    }

    return (mv);
}

// This function differs from the above in that it's used to set the
// initial time_taken value for the turn.  Everything else (movement,
// spellcasting, combat) applies a ratio to this value.
int player_speed(void)
{
    int ps = 10;

    if (you.haste)
        ps /= 2;

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_STATUE:
        ps *= 15;
        ps /= 10;
        break;

    case TRAN_SERPENT_OF_HELL:
        ps *= 12;
        ps /= 10;
        break;

    default:
        break;
    }

    return ps;
}

int player_AC(void)
{
    int AC = 0;
    int i;                      // loop variable

    // get the armour race value that corresponds to the character's race:
    const unsigned long racial_type 
                            = ((player_genus(GENPC_DWARVEN)) ? ISFLAG_DWARVEN :
                               (player_genus(GENPC_ELVEN))   ? ISFLAG_ELVEN :
                               (you.species == SP_HILL_ORC)  ? ISFLAG_ORCISH
                                                             : 0);

    for (i = EQ_CLOAK; i <= EQ_BODY_ARMOUR; i++)
    {
        const int item = you.equip[i]; 

        if (item == -1 || i == EQ_SHIELD)
            continue;

        AC += you.inv[ item ].plus;

        // Note that helms and boots have a sub-sub classing system
        // which uses "plus2"... since not all members have the same
        // AC value, we use special cases. -- bwr
        if (i == EQ_HELMET 
            && (get_helmet_type(you.inv[ item ]) == THELM_CAP
                || get_helmet_type(you.inv[ item ]) == THELM_WIZARD_HAT
                || get_helmet_type(you.inv[ item ]) == THELM_SPECIAL))
        {
            continue;
        }

        int racial_bonus = 0;  // additional levels of armour skill
        const unsigned long armour_race = get_equip_race( you.inv[ item ] );
        const int ac_value = property( you.inv[ item ], PARM_AC );

        // Dwarven armour is universally good -- bwr
        if (armour_race == ISFLAG_DWARVEN)
            racial_bonus += 2;

        if (racial_type && armour_race == racial_type)
        {
            // Elven armour is light, but still gives one level 
            // to elves.  Orcish and Dwarven armour are worth +2
            // to the correct species, plus the plus that anyone
            // gets with dwarven armour. -- bwr

            if (racial_type == ISFLAG_ELVEN)
                racial_bonus++;
            else
                racial_bonus += 2;
        }

        AC += ac_value * (15 + you.skills[SK_ARMOUR] + racial_bonus) / 15;

        /* The deformed don't fit into body armour very well
           (this includes nagas and centaurs) */
        if (i == EQ_BODY_ARMOUR && you.mutation[MUT_DEFORMED])
            AC -= ac_value / 2;
    }

    AC += player_equip( EQ_RINGS_PLUS, RING_PROTECTION );

    if (player_equip_ego_type( EQ_WEAPON, SPWPN_PROTECTION ))
        AC += 5;

    if (player_equip_ego_type( EQ_SHIELD, SPARM_PROTECTION ))
        AC += 3;

    AC += scan_randarts(RAP_AC);

    if (you.duration[DUR_ICY_ARMOUR])
        AC += 4 + you.skills[SK_ICE_MAGIC] / 3;         // max 13

    if (you.duration[DUR_STONEMAIL])
        AC += 5 + you.skills[SK_EARTH_MAGIC] / 2;       // max 18

    if (you.duration[DUR_STONESKIN])
        AC += 2 + you.skills[SK_EARTH_MAGIC] / 5;       // max 7

    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_NONE
        || you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH
        || you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
    {
        // Being a lich doesn't preclude the benefits of hide/scales -- bwr
        //
        // Note: Even though necromutation is a high level spell, it does 
        // allow the character full armour (so the bonus is low). -- bwr
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH)
            AC += (3 + you.skills[SK_NECROMANCY] / 6);          // max 7

        //jmf: only give:
        if (player_genus(GENPC_DRACONIAN))
        {
            if (you.experience_level < 8)
                AC += 2;
            else if (you.species == SP_GREY_DRACONIAN)
                AC += 1 + (you.experience_level - 4) / 2;       // max 12
            else
                AC += 1 + (you.experience_level / 4);           // max 7
        }
        else
        {
            switch (you.species)
            {
            case SP_NAGA:
                AC += you.experience_level / 3;                 // max 9
                break;

            case SP_OGRE:
                AC++;
                break;

            case SP_TROLL:
            case SP_CENTAUR:
                AC += 3;
                break;

            default:
                break;
            }
        }

        // Scales -- some evil uses of the fact that boolean "true" == 1...
        // I'll spell things out with some comments -- bwr

        // mutations:
        // these give: +1, +2, +3
        AC += you.mutation[MUT_TOUGH_SKIN];
        AC += you.mutation[MUT_GREY_SCALES];
        AC += you.mutation[MUT_SPECKLED_SCALES];
        AC += you.mutation[MUT_IRIDESCENT_SCALES];
        AC += you.mutation[MUT_PATTERNED_SCALES];
        AC += you.mutation[MUT_BLUE_SCALES];

        // these gives: +1, +3, +5
        if (you.mutation[MUT_GREEN_SCALES] > 0)
            AC += (you.mutation[MUT_GREEN_SCALES] * 2) - 1;
        if (you.mutation[MUT_NACREOUS_SCALES] > 0)
            AC += (you.mutation[MUT_NACREOUS_SCALES] * 2) - 1;
        if (you.mutation[MUT_BLACK2_SCALES] > 0)
            AC += (you.mutation[MUT_BLACK2_SCALES] * 2) - 1;
        if (you.mutation[MUT_WHITE_SCALES] > 0)
            AC += (you.mutation[MUT_WHITE_SCALES] * 2) - 1;

        // these give: +2, +4, +6
        AC += you.mutation[MUT_GREY2_SCALES] * 2;
        AC += you.mutation[MUT_YELLOW_SCALES] * 2;
        AC += you.mutation[MUT_PURPLE_SCALES] * 2;

        // black gives: +3, +6, +9
        AC += you.mutation[MUT_BLACK_SCALES] * 3;

        // boney plates give: +2, +3, +4
        if (you.mutation[MUT_BONEY_PLATES] > 0)
            AC += you.mutation[MUT_BONEY_PLATES] + 1;

        // red gives +1, +2, +4
        AC += you.mutation[MUT_RED_SCALES]
                            + (you.mutation[MUT_RED_SCALES] == 3);

        // indigo gives: +2, +3, +5
        if (you.mutation[MUT_INDIGO_SCALES] > 0)
        {
            AC += 1 + you.mutation[MUT_INDIGO_SCALES]
                            + (you.mutation[MUT_INDIGO_SCALES] == 3);
        }

        // brown gives: +2, +4, +5
        AC += (you.mutation[MUT_BROWN_SCALES] * 2)
                            - (you.mutation[MUT_METALLIC_SCALES] == 3);

        // orange gives: +1, +3, +4
        AC += you.mutation[MUT_ORANGE_SCALES]
                            + (you.mutation[MUT_ORANGE_SCALES] > 1);

        // knobbly red gives: +2, +5, +7
        AC += (you.mutation[MUT_RED2_SCALES] * 2)
                            + (you.mutation[MUT_RED2_SCALES] > 1);

        // metallic gives +3, +7, +10
        AC += you.mutation[MUT_METALLIC_SCALES] * 3
                            + (you.mutation[MUT_METALLIC_SCALES] > 1);
    }
    else
    {
        // transformations:
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_NONE:
        case TRAN_BLADE_HANDS:
        case TRAN_LICH:  // can wear normal body armour (small bonus)
            break;


        case TRAN_SPIDER: // low level (small bonus), also gets EV
            AC += (2 + you.skills[SK_POISON_MAGIC] / 6);        // max 6
            break;

        case TRAN_ICE_BEAST:
            AC += (5 + (you.skills[SK_ICE_MAGIC] + 1) / 4);     // max 12

            if (you.duration[DUR_ICY_ARMOUR])
                AC += (1 + you.skills[SK_ICE_MAGIC] / 4);       // max +7 
            break;

        case TRAN_DRAGON:
            AC += (7 + you.skills[SK_FIRE_MAGIC] / 3);          // max 16
            break;

        case TRAN_STATUE: // main ability is armour (high bonus)
            AC += (17 + you.skills[SK_EARTH_MAGIC] / 2);        // max 30

            if (you.duration[DUR_STONESKIN] || you.duration[DUR_STONEMAIL])
                AC += (1 + you.skills[SK_EARTH_MAGIC] / 4);     // max +7
            break;

        case TRAN_SERPENT_OF_HELL:
            AC += (10 + you.skills[SK_FIRE_MAGIC] / 3);         // max 19
            break;

        case TRAN_AIR:    // air - scales & species ought to be irrelevent
            AC = (you.skills[SK_AIR_MAGIC] * 3) / 2;            // max 40
            break;

        default:
            break;
        }
    }

    return AC;
}

bool is_light_armour( const item_def &item )
{
    if (get_equip_race(item) == ISFLAG_ELVEN)
        return (true);

    switch (item.sub_type)
    {
    case ARM_ROBE:
    case ARM_ANIMAL_SKIN:
    case ARM_LEATHER_ARMOUR:
    case ARM_STEAM_DRAGON_HIDE:
    case ARM_STEAM_DRAGON_ARMOUR:
    case ARM_MOTTLED_DRAGON_HIDE:
    case ARM_MOTTLED_DRAGON_ARMOUR:
    //case ARM_TROLL_HIDE: //jmf: these are knobbly and stiff
    //case ARM_TROLL_LEATHER_ARMOUR:
        return (true);

    default:
        return (false);
    }
}

bool player_light_armour(bool with_skill)
{
    const int arm = you.equip[EQ_BODY_ARMOUR];

    if (arm == -1)
        return true;

    if (with_skill &&
        property(you.inv[arm], PARM_EVASION) + you.skills[SK_ARMOUR]/3 >= 0)
        return true;

    return (is_light_armour(you.inv[arm]));
}                               // end player_light_armour()

//
// This function returns true if the player has a radically different
// shape... minor changes like blade hands don't count, also note
// that lich transformation doesn't change the character's shape
// (so we end up with Naga-lichs, Spiggan-lichs, Minotaur-lichs)
// it just makes the character undead (with the benefits that implies). --bwr
//
bool player_is_shapechanged(void)
{
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_NONE
        || you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS
        || you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH)
    {
        return (false);
    }

    return (true);
}

// psize defaults to PSIZE_TORSO, which checks the part of the body 
// that wears armour and wields weapons (which is different for some hybrids).
// base defaults to "false", meaning consider our current size, not our
// natural one.
size_type player_size( int psize, bool base )
{
    return you.body_size(psize, base);
}

// New and improved 4.1 evasion model, courtesy Brent Ross.
int player_evasion()
{
    // XXX: player_size() implementations are incomplete, fix.
    const size_type size  = player_size(PSIZE_BODY);
    const size_type torso = player_size(PSIZE_TORSO);

    const int size_factor = SIZE_MEDIUM - size;
    int ev = 10 + 2 * size_factor;

    // Repulsion fields and size are all that matters when paralysed.
    if (you.paralysed())
    {
        ev = 2 + size_factor;
        if (you.mutation[MUT_REPULSION_FIELD] > 0)
            ev += (you.mutation[MUT_REPULSION_FIELD] * 2) - 1;
        return ((ev < 1) ? 1 : ev);
    }

    // Calculate the base bonus here, but it may be reduced by heavy 
    // armour below.
    int dodge_bonus = (you.skills[SK_DODGING] * you.dex + 7) 
                      / (20 - size_factor);

    // Limit on bonus from dodging:
    const int max_bonus = (you.skills[SK_DODGING] * (7 + size_factor)) / 9;
    if (dodge_bonus > max_bonus)
        dodge_bonus = max_bonus;

    // Some lesser armours have small penalties now (shields, barding)
    for (int i = EQ_CLOAK; i < EQ_BODY_ARMOUR; i++)
    {
        if (you.equip[i] == -1)
            continue;

        int pen = property( you.inv[ you.equip[i] ], PARM_EVASION );

        // reducing penalty of larger shields for larger characters
        if (i == EQ_SHIELD && torso > SIZE_MEDIUM)
            pen += (torso - SIZE_MEDIUM);

        if (pen < 0)
            ev += pen;
    }

    // handle main body armour penalty
    if (you.equip[EQ_BODY_ARMOUR] != -1)
    {
        // XXX: magnify arm_penalty for weak characters?
        // Remember: arm_penalty and ev_change are negative.
        int arm_penalty = property( you.inv[ you.equip[EQ_BODY_ARMOUR] ],
                                          PARM_EVASION );

        // The very large races take a penalty to AC for not being able
        // to fully cover, and in compensation we give back some freedom
        // of movement here.  Likewise, the very small are extra encumbered
        // by armour (which partially counteracts their size bonus above).
        if (size < SIZE_SMALL || size > SIZE_LARGE)
        {
            arm_penalty -= ((size - SIZE_MEDIUM) * arm_penalty) / 4;

            if (arm_penalty > 0)
                arm_penalty = 0;
        }

        int ev_change = arm_penalty;
        ev_change += (you.skills[SK_ARMOUR] * you.strength) / 60;

        if (ev_change > arm_penalty / 2)
            ev_change = arm_penalty / 2;

        ev += ev_change;

        // This reduces dodging ability in heavy armour.
        if (!player_light_armour())
            dodge_bonus += (arm_penalty * 30 + 15) / you.strength;
    }

    if (dodge_bonus > 0)                // always a bonus
        ev += dodge_bonus;

    if (you.duration[DUR_FORESCRY])
        ev += 8;

    if (you.duration[DUR_STONEMAIL])
        ev -= 2;

    ev += player_equip( EQ_RINGS_PLUS, RING_EVASION );
    ev += scan_randarts( RAP_EVASION );

    if (player_equip_ego_type( EQ_BODY_ARMOUR, SPARM_PONDEROUSNESS ))
        ev -= 2;

    if (you.mutation[MUT_REPULSION_FIELD] > 0)
        ev += (you.mutation[MUT_REPULSION_FIELD] * 2) - 1;

    // transformation penalties/bonuses not covered by size alone:
    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_STATUE:
        ev -= 5;                // stiff
        break;
        
    case TRAN_AIR:
        ev += 20;               // vapourous
        break;

    default:
        break;
    }

    return (ev);
}

// Obsolete evasion calc.
#if 0
int old_player_evasion(void)
{
    int ev = 10;

    int armour_ev_penalty;

    if (you.equip[EQ_BODY_ARMOUR] == -1)
        armour_ev_penalty = 0;
    else
    {
        armour_ev_penalty = property( you.inv[you.equip[EQ_BODY_ARMOUR]],
                                      PARM_EVASION );
    }

    // We return 2 here to give the player some chance of not being hit,
    // repulsion fields still work while paralysed
    if (you.paralysis)
        return (2 + you.mutation[MUT_REPULSION_FIELD] * 2);

    if (you.species == SP_CENTAUR)
        ev -= 3;

    if (you.equip[EQ_BODY_ARMOUR] != -1)
    {
        int ev_change = 0;

        ev_change = armour_ev_penalty;
        ev_change += you.skills[SK_ARMOUR] / 3;

        if (ev_change > armour_ev_penalty / 3)
            ev_change = armour_ev_penalty / 3;

        ev += ev_change;        /* remember that it's negative */
    }

    ev += player_equip( EQ_RINGS_PLUS, RING_EVASION );

    if (player_equip_ego_type( EQ_BODY_ARMOUR, SPARM_PONDEROUSNESS ))
        ev -= 2;

    if (you.duration[DUR_STONEMAIL])
        ev -= 2;

    if (you.duration[DUR_FORESCRY])
        ev += 8;                //jmf: is this a reasonable value?

    int emod = 0;

    if (!player_light_armour())
    {
        // meaning that the armour evasion modifier is often effectively
        // applied twice, but not if you're wearing elven armour
        emod += (armour_ev_penalty * 14) / 10;
    }

    emod += you.skills[SK_DODGING] / 2;

    if (emod > 0)
        ev += emod;

    if (you.mutation[MUT_REPULSION_FIELD] > 0)
        ev += (you.mutation[MUT_REPULSION_FIELD] * 2) - 1;

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_DRAGON:
        ev -= 3;
        break;

    case TRAN_STATUE:
    case TRAN_SERPENT_OF_HELL:
        ev -= 5;
        break;

    case TRAN_SPIDER:
        ev += 3;
        break;

    case TRAN_AIR:
        ev += 20;
        break;

    default:
        break;
    }

    ev += scan_randarts(RAP_EVASION);

    return ev;
}                               // end player_evasion()
#endif

int player_magical_power( void )
{
    int ret = 0;

    ret += 13 * player_equip( EQ_STAFF, STAFF_POWER );
    ret +=  9 * player_equip( EQ_RINGS, RING_MAGICAL_POWER );
    
    return (ret);
}

int player_mag_abil(bool is_weighted)
{
    int ma = 0;

    ma += 3 * player_equip( EQ_RINGS, RING_WIZARDRY );

    /* Staves */
    ma += 4 * player_equip( EQ_STAFF, STAFF_WIZARDRY );

    /* armour of the Archmagi (checks body armour only) */
    ma += 2 * player_equip_ego_type( EQ_BODY_ARMOUR, SPARM_ARCHMAGI );

    return ((is_weighted) ? ((ma * you.intel) / 10) : ma);
}                               // end player_mag_abil()

// Returns the shield the player is wearing, or NULL if none.
item_def *player_shield()
{
    return you.shield();
}

int player_shield_class(void)   //jmf: changes for new spell
{
    int base_shield = 0;
    const int shield = you.equip[EQ_SHIELD];

    if (shield == -1)
    {
        if (!you.fire_shield && you.duration[DUR_CONDENSATION_SHIELD])
            base_shield = 2 + (you.skills[SK_ICE_MAGIC] / 6);  // max 6
        else
            return (0);
    }
    else
    {
        switch (you.inv[ shield ].sub_type)
        {
        case ARM_BUCKLER:
            base_shield = 3;   // +3/20 per skill level    max 7
            break;
        case ARM_SHIELD:
            base_shield = 5;   // +5/20 per skill level    max 11
            break;
        case ARM_LARGE_SHIELD:
            base_shield = 7;   // +7/20 per skill level    max 16
            break;
        }

        // bonus applied only to base, see above for effect:
        base_shield *= (20 + you.skills[SK_SHIELDS]);
        base_shield /= 20;

        base_shield += you.inv[ shield ].plus;
    }

    return (base_shield);
}                               // end player_shield_class()

int player_see_invis(bool calc_unid)
{
    int si = 0;

    si += player_equip( EQ_RINGS, RING_SEE_INVISIBLE, calc_unid );

    /* armour: (checks head armour only) */
    si += player_equip_ego_type( EQ_HELMET, SPARM_SEE_INVISIBLE );

    if (you.mutation[MUT_ACUTE_VISION] > 0)
        si += you.mutation[MUT_ACUTE_VISION];

    //jmf: added see_invisible spell
    if (you.duration[DUR_SEE_INVISIBLE] > 0)
        si++;

    /* randart wpns */
    int artefacts = scan_randarts(RAP_EYESIGHT, calc_unid);

    if (artefacts > 0)
        si += artefacts;

    if ( si > 1 )
        si = 1;

    return si;
}

// This does NOT do line of sight!  It checks the monster's visibility 
// with repect to the players perception, but doesn't do walls or range...
// to find if the square the monster is in is visible see mons_near().
bool player_monster_visible( const monsters *mon )
{
    if (mon->has_ench(ENCH_SUBMERGED)
        || (mon->has_ench(ENCH_INVIS) && !player_see_invis()))
    {
        return (false);
    }

    return (true);
}

int player_sust_abil(bool calc_unid)
{
    int sa = 0;

    sa += player_equip( EQ_RINGS, RING_SUSTAIN_ABILITIES, calc_unid );

    return sa;
}                               // end player_sust_abil()

int carrying_capacity( burden_state_type bs )
{
    int cap = 3500+(you.strength * 100)+(player_is_levitating() ? 1000 : 0);
    if ( bs == BS_UNENCUMBERED )
        return (cap * 5) / 6;
    else if ( bs == BS_ENCUMBERED )
        return (cap * 11) / 12;
    else
        return cap;
}

int burden_change(void)
{
    char old_burdenstate = you.burden_state;

    you.burden = 0;

    if (you.duration[DUR_STONEMAIL])
        you.burden += 800;

    for (unsigned char bu = 0; bu < ENDOFPACK; bu++)
    {
        if (you.inv[bu].quantity < 1)
            continue;

        if (you.inv[bu].base_type == OBJ_CORPSES)
        {
            if (you.inv[bu].sub_type == CORPSE_BODY)
                you.burden += mons_weight(you.inv[bu].plus);
            else if (you.inv[bu].sub_type == CORPSE_SKELETON)
                you.burden += mons_weight(you.inv[bu].plus) / 2;
        }
        else
        {
            you.burden += item_mass( you.inv[bu] ) * you.inv[bu].quantity;
        }
    }

    you.burden_state = BS_UNENCUMBERED;
    set_redraw_status( REDRAW_BURDEN );

    // changed the burdened levels to match the change to max_carried
    if (you.burden < carrying_capacity(BS_UNENCUMBERED))
    {
        you.burden_state = BS_UNENCUMBERED;

        // this message may have to change, just testing {dlb}
        if (old_burdenstate != you.burden_state)
            mpr("Your possessions no longer seem quite so burdensome.");
    }
    else if (you.burden < carrying_capacity(BS_ENCUMBERED))
    {
        you.burden_state = BS_ENCUMBERED;

        if (old_burdenstate != you.burden_state) {
            mpr("You are being weighed down by all of your possessions.");
            learned_something_new(TUT_HEAVY_LOAD);
        }    
    }
    else
    {
        you.burden_state = BS_OVERLOADED;

        if (old_burdenstate != you.burden_state)
            mpr("You are being crushed by all of your possessions.");
    }

    // Stop travel if we get burdened (as from potions of might/levitation
    // wearing off).
    if (you.burden_state > old_burdenstate)
        interrupt_activity( AI_BURDEN_CHANGE );

    return you.burden;
}                               // end burden_change()

bool you_resist_magic(int power)
{
    int ench_power = stepdown_value( power, 30, 40, 100, 120 );

    int mrchance = 100 + player_res_magic();

    mrchance -= ench_power;

    int mrch2 = random2(100) + random2(101);

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "Power: %d, player's MR: %d, target: %d, roll: %d", 
             ench_power, player_res_magic(), mrchance, mrch2 ); 

    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (mrch2 < mrchance)
        return true;            // ie saved successfully

    return false;
/* if (random2(power) / 3 + random2(power) / 3 + random2(power) / 3 >= player_res_magic()) return 0;
   return 1; */
}

void forget_map(unsigned char chance_forgotten)
{
    unsigned char xcount, ycount = 0;

    for (xcount = 0; xcount < GXM; xcount++)
    {
        for (ycount = 0; ycount < GYM; ycount++)
        {
            if (random2(100) < chance_forgotten)
            {
                env.map[xcount][ycount] = 0;
                env.map_col[xcount][ycount].clear();
            }
        }
    }
}                               // end forget_map()

void gain_exp( unsigned int exp_gained )
{

    if (player_equip_ego_type( EQ_BODY_ARMOUR, SPARM_ARCHMAGI ) 
        && !one_chance_in(20))
    {
        return;
    }

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "gain_exp: %d", exp_gained );
    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (you.experience + exp_gained > 8999999)
        you.experience = 8999999;
    else
        you.experience += exp_gained;

    if (you.exp_available + exp_gained > 20000)
        you.exp_available = 20000;
    else
        you.exp_available += exp_gained;

    level_change();
    if (Options.tutorial_left && you.experience_level == 5)
        tutorial_finished();
}                               // end gain_exp()

void level_change(void)
{
    int hp_adjust = 0;
    int mp_adjust = 0;

    // necessary for the time being, as level_change() is called
    // directly sometimes {dlb}
    you.redraw_experience = 1;

    while (you.experience_level < 27
            && you.experience > exp_needed(you.experience_level + 2))
    {
        you.experience_level++;

        if (you.experience_level <= you.max_level)
        {
            snprintf( info, INFO_SIZE, "Welcome back to level %d!",
                      you.experience_level );

            mpr(info, MSGCH_INTRINSIC_GAIN);
            more();

            // Gain back the hp and mp we lose in lose_level().  -- bwr
            inc_hp( 4, true );
            inc_mp( 1, true );
        }
        else  // character has gained a new level
        {
            snprintf( info, INFO_SIZE, "You are now a level %d %s!", 
                      you.experience_level, you.class_name );

            mpr(info, MSGCH_INTRINSIC_GAIN);
            more();

            int brek = 0;

            if (you.experience_level > 21)
                brek = (coinflip() ? 3 : 2);
            else if (you.experience_level > 12)
                brek = 3 + random2(3);      // up from 2 + rand(3) -- bwr
            else
                brek = 4 + random2(4);      // up from 3 + rand(4) -- bwr

            inc_hp( brek, true );
            inc_mp( 1, true );

            if (!(you.experience_level % 3))
                ability_increase();

            switch (you.species)
            {
            case SP_HUMAN:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_RANDOM, 1, false);
                break;

            case SP_ELF:
                if (you.experience_level % 3)
                    hp_adjust--;
                else
                    mp_adjust++;

                if (!(you.experience_level % 4))
                {
                    modify_stat( (coinflip() ? STAT_INTELLIGENCE
                                                : STAT_DEXTERITY), 1, false );
                }
                break;

            case SP_HIGH_ELF:
                if (you.experience_level == 15)
                {
                    //jmf: got Glamour ability
                    mpr("You feel charming!", MSGCH_INTRINSIC_GAIN);
                }

                if (you.experience_level % 3)
                    hp_adjust--;

                if (!(you.experience_level % 2))
                    mp_adjust++;

                if (!(you.experience_level % 3))
                {
                    modify_stat( (coinflip() ? STAT_INTELLIGENCE
                                                : STAT_DEXTERITY), 1, false );
                }
                break;

            case SP_GREY_ELF:
                if (you.experience_level == 5)
                {
                    //jmf: got Glamour ability
                    mpr("You feel charming!", MSGCH_INTRINSIC_GAIN);
                    mp_adjust++;
                }

                if (you.experience_level < 14)
                    hp_adjust--;

                if (you.experience_level % 3)
                    mp_adjust++;

                if (!(you.experience_level % 4))
                {
                    modify_stat( (coinflip() ? STAT_INTELLIGENCE
                                                : STAT_DEXTERITY), 1, false );
                }

                break;

            case SP_DEEP_ELF:
                if (you.experience_level < 17)
                    hp_adjust--;
                if (!(you.experience_level % 3))
                    hp_adjust--;

                mp_adjust++;

                if (!(you.experience_level % 4))
                    modify_stat(STAT_INTELLIGENCE, 1, false);
                break;

            case SP_SLUDGE_ELF:
                if (you.experience_level % 3)
                    hp_adjust--;
                else
                    mp_adjust++;

                if (!(you.experience_level % 4))
                {
                    modify_stat( (coinflip() ? STAT_INTELLIGENCE
                                                : STAT_DEXTERITY), 1, false );
                }
                break;

            case SP_HILL_DWARF:
                // lowered because of HD raise -- bwr
                // if (you.experience_level < 14)
                //     hp_adjust++;

                if (you.experience_level % 3)
                    hp_adjust++;

                if (!(you.experience_level % 2))
                    mp_adjust--;

                if (!(you.experience_level % 4))
                    modify_stat(STAT_STRENGTH, 1, false);
                break;

            case SP_MOUNTAIN_DWARF:
                // lowered because of HD raise -- bwr
                // if (you.experience_level < 14)
                //     hp_adjust++;

                if (!(you.experience_level % 2))
                    hp_adjust++;

                if (!(you.experience_level % 3))
                    mp_adjust--;

                if (!(you.experience_level % 4))
                    modify_stat(STAT_STRENGTH, 1, false);
                break;

            case SP_HALFLING:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_DEXTERITY, 1, false);

                if (you.experience_level < 17)
                    hp_adjust--;

                if (!(you.experience_level % 2))
                    hp_adjust--;
                break;

            case SP_KOBOLD:
                if (!(you.experience_level % 5))
                {
                    modify_stat( (coinflip() ? STAT_STRENGTH
                                                : STAT_DEXTERITY), 1, false );
                }

                if (you.experience_level < 17)
                    hp_adjust--;

                if (!(you.experience_level % 2))
                    hp_adjust--;
                break;

            case SP_HILL_ORC:
                // lower because of HD raise -- bwr
                // if (you.experience_level < 17)
                //     hp_adjust++;

                if (!(you.experience_level % 2))
                    hp_adjust++;

                if (!(you.experience_level % 3))
                    mp_adjust--;

                if (!(you.experience_level % 5))
                    modify_stat(STAT_STRENGTH, 1, false);
                break;

            case SP_MUMMY:
                if (you.experience_level == 13 || you.experience_level == 26)
                {
                    mpr( "You feel more in touch with the powers of death.",
                         MSGCH_INTRINSIC_GAIN );
                }

                if (you.experience_level == 13)  // level 13 for now -- bwr
                {
                    mpr( "You can now infuse your body with magic to restore decomposition.", MSGCH_INTRINSIC_GAIN );
                }
                break;

            case SP_NAGA:
                // lower because of HD raise -- bwr
                // if (you.experience_level < 14)
                //     hp_adjust++;

                hp_adjust++;

                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false);

                if (!(you.experience_level % 3))
                {
                    mpr("Your skin feels tougher.", MSGCH_INTRINSIC_GAIN);
                    you.redraw_armour_class = 1;
                }
                break;

            case SP_GNOME:
                if (you.experience_level < 13)
                    hp_adjust--;

                if (!(you.experience_level % 3))
                    hp_adjust--;

                if (!(you.experience_level % 4))
                {
                    modify_stat( (coinflip() ? STAT_INTELLIGENCE
                                                : STAT_DEXTERITY), 1, false );
                }
                break;

            case SP_OGRE:
            case SP_TROLL:
                hp_adjust++;

                // lowered because of HD raise -- bwr
                // if (you.experience_level < 14)
                //     hp_adjust++;

                if (!(you.experience_level % 2))
                    hp_adjust++;

                if (you.experience_level % 3)
                    mp_adjust--;

                if (!(you.experience_level % 3))
                    modify_stat(STAT_STRENGTH, 1, false);
                break;

            case SP_OGRE_MAGE:
                hp_adjust++;

                // lowered because of HD raise -- bwr
                // if (you.experience_level < 14)
                //     hp_adjust++;

                if (!(you.experience_level % 5))
                {
                    modify_stat( (coinflip() ? STAT_INTELLIGENCE
                                                : STAT_STRENGTH), 1, false );
                }
                break;

            case SP_RED_DRACONIAN:
            case SP_WHITE_DRACONIAN:
            case SP_GREEN_DRACONIAN:
            case SP_GOLDEN_DRACONIAN:
/* Grey is later */
            case SP_BLACK_DRACONIAN:
            case SP_PURPLE_DRACONIAN:
            case SP_MOTTLED_DRACONIAN:
            case SP_PALE_DRACONIAN:
            case SP_UNK0_DRACONIAN:
            case SP_UNK1_DRACONIAN:
            case SP_BASE_DRACONIAN:
                if (you.experience_level == 7)
                {
                    switch (you.species)
                    {
                    case SP_RED_DRACONIAN:
                        mpr("Your scales start taking on a fiery red colour.",
                            MSGCH_INTRINSIC_GAIN);
                        break;
                    case SP_WHITE_DRACONIAN:
                        mpr("Your scales start taking on an icy white colour.",
                            MSGCH_INTRINSIC_GAIN);
                        break;
                    case SP_GREEN_DRACONIAN:
                        mpr("Your scales start taking on a green colour.",
                            MSGCH_INTRINSIC_GAIN);
                        // green dracos get this at level 7
                        perma_mutate(MUT_POISON_RESISTANCE, 1);
                        break;

                    case SP_GOLDEN_DRACONIAN:
                        mpr("Your scales start taking on a golden yellow colour.", MSGCH_INTRINSIC_GAIN);
                        break;
                    case SP_BLACK_DRACONIAN:
                        mpr("Your scales start turning black.", MSGCH_INTRINSIC_GAIN);
                        break;
                    case SP_PURPLE_DRACONIAN:
                        mpr("Your scales start taking on a rich purple colour.", MSGCH_INTRINSIC_GAIN);
                        break;
                    case SP_MOTTLED_DRACONIAN:
                        mpr("Your scales start taking on a weird mottled pattern.", MSGCH_INTRINSIC_GAIN);
                        break;
                    case SP_PALE_DRACONIAN:
                        mpr("Your scales start fading to a pale grey colour.", MSGCH_INTRINSIC_GAIN);
                        break;
                    case SP_UNK0_DRACONIAN:
                    case SP_UNK1_DRACONIAN:
                    case SP_BASE_DRACONIAN:
                        mpr("");
                        break;
                    }
                    more();
                    redraw_screen();
                }

                if (you.experience_level == 18)
                {
                    switch (you.species)
                    {
                    case SP_RED_DRACONIAN:
                        perma_mutate(MUT_HEAT_RESISTANCE, 1);
                        break;
                    case SP_WHITE_DRACONIAN:
                        perma_mutate(MUT_COLD_RESISTANCE, 1);
                        break;
                    case SP_BLACK_DRACONIAN:
                        perma_mutate(MUT_SHOCK_RESISTANCE, 1);
                        break;
                    default:
                        break;
                    }
                }

                if (!(you.experience_level % 3))
                    hp_adjust++;

                if (you.experience_level > 7 && !(you.experience_level % 4))
                {
                    mpr("Your scales feel tougher.", MSGCH_INTRINSIC_GAIN);
                    you.redraw_armour_class = 1;
                    modify_stat(STAT_RANDOM, 1, false);
                }
                break;

            case SP_GREY_DRACONIAN:
                if (you.experience_level == 7)
                {
                    mpr("Your scales start turning grey.", MSGCH_INTRINSIC_GAIN);
                    more();
                    redraw_screen();
                }

                if (!(you.experience_level % 3))
                {
                    hp_adjust++;
                    if (you.experience_level > 7)
                        hp_adjust++;
                }

                if (you.experience_level > 7 && !(you.experience_level % 2))
                {
                    mpr("Your scales feel tougher.", MSGCH_INTRINSIC_GAIN);
                    you.redraw_armour_class = 1;
                }

                if ((you.experience_level > 7 && !(you.experience_level % 3))
                    || you.experience_level == 4 || you.experience_level == 7)
                {
                    modify_stat(STAT_RANDOM, 1, false);
                }
                break;

            case SP_CENTAUR:
                if (!(you.experience_level % 4))
                {
                    modify_stat( (coinflip() ? STAT_DEXTERITY
                                                : STAT_STRENGTH), 1, false );
                }

                // lowered because of HD raise -- bwr
                // if (you.experience_level < 17)
                //     hp_adjust++;

                if (!(you.experience_level % 2))
                    hp_adjust++;

                if (!(you.experience_level % 3))
                    mp_adjust--;
                break;

            case SP_DEMIGOD:
                if (!(you.experience_level % 3))
                    modify_stat(STAT_RANDOM, 1, false);

                // lowered because of HD raise -- bwr
                // if (you.experience_level < 17)
                //    hp_adjust++;

                if (!(you.experience_level % 2))
                    hp_adjust++;

                if (you.experience_level % 3)
                    mp_adjust++;
                break;

            case SP_SPRIGGAN:
                if (you.experience_level < 17)
                    hp_adjust--;

                if (you.experience_level % 3)
                    hp_adjust--;

                mp_adjust++;

                if (!(you.experience_level % 5))
                {
                    modify_stat( (coinflip() ? STAT_INTELLIGENCE
                                                : STAT_DEXTERITY), 1, false );
                }
                break;

            case SP_MINOTAUR:
                // lowered because of HD raise -- bwr
                // if (you.experience_level < 17)
                //     hp_adjust++;

                if (!(you.experience_level % 2))
                    hp_adjust++;

                if (!(you.experience_level % 2))
                    mp_adjust--;

                if (!(you.experience_level % 4))
                {
                    modify_stat( (coinflip() ? STAT_DEXTERITY
                                                : STAT_STRENGTH), 1, false );
                }
                break;

            case SP_DEMONSPAWN:
                if (you.attribute[ATTR_NUM_DEMONIC_POWERS] == 0
                    && (you.experience_level == 4
                        || (you.experience_level < 4 && one_chance_in(3))))
                {
                    demonspawn();
                }

                if (you.attribute[ATTR_NUM_DEMONIC_POWERS] == 1
                    && you.experience_level > 4
                    && (you.experience_level == 9
                        || (you.experience_level < 9 && one_chance_in(3))))
                {
                    demonspawn();
                }

                if (you.attribute[ATTR_NUM_DEMONIC_POWERS] == 2
                    && you.experience_level > 9
                    && (you.experience_level == 14
                        || (you.experience_level < 14 && one_chance_in(3))))
                {
                    demonspawn();
                }

                if (you.attribute[ATTR_NUM_DEMONIC_POWERS] == 3
                    && you.experience_level > 14
                    && (you.experience_level == 19
                        || (you.experience_level < 19 && one_chance_in(3))))
                {
                    demonspawn();
                }

                if (you.attribute[ATTR_NUM_DEMONIC_POWERS] == 4
                    && you.experience_level > 19
                    && (you.experience_level == 24
                        || (you.experience_level < 24 && one_chance_in(3))))
                {
                    demonspawn();
                }

                if (you.attribute[ATTR_NUM_DEMONIC_POWERS] == 5
                    && you.experience_level == 27)
                {
                    demonspawn();
                }

                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false);
                break;

            case SP_GHOUL:
                // lowered because of HD raise -- bwr
                // if (you.experience_level < 17)
                //     hp_adjust++;

                if (!(you.experience_level % 2))
                    hp_adjust++;

                if (!(you.experience_level % 3))
                    mp_adjust--;

                if (!(you.experience_level % 5))
                    modify_stat(STAT_STRENGTH, 1, false);
                break;

            case SP_KENKU:
                if (you.experience_level < 17)
                    hp_adjust--;

                if (!(you.experience_level % 3))
                    hp_adjust--;

                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false);

                if (you.experience_level == 5)
                    mpr("You have gained the ability to fly.", MSGCH_INTRINSIC_GAIN);
                else if (you.experience_level == 15)
                    mpr("You can now fly continuously.", MSGCH_INTRINSIC_GAIN);
                break;

            case SP_MERFOLK:
                if (you.experience_level % 3)
                    hp_adjust++;

                if (!(you.experience_level % 5))
                    modify_stat(STAT_RANDOM, 1, false);
                break;
            }
        }

        // add hp and mp adjustments - GDL
        inc_max_hp( hp_adjust );
        inc_max_mp( mp_adjust );

        deflate_hp( you.hp_max, false );

        if (you.magic_points < 0)
            you.magic_points = 0;

        calc_hp();
        calc_mp();

        if (you.experience_level > you.max_level)
            you.max_level = you.experience_level;
        
        char buf[200];
        sprintf(buf, "HP: %d/%d MP: %d/%d",
                you.hp, you.hp_max, you.magic_points, you.max_magic_points);
        take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0, buf));

        if (you.religion == GOD_XOM)
            Xom_acts(true, you.experience_level, true);
            
        learned_something_new(TUT_NEW_LEVEL);   
            
    }

    redraw_skill( you.your_name, player_title() );

}                               // end level_change()

// here's a question for you: does the ordering of mods make a difference?
// (yes) -- are these things in the right order of application to stealth?
// - 12mar2000 {dlb}
int check_stealth(void)
{
    if (you.special_wield == SPWLD_SHADOW || you.berserker)
        return (0);

    int stealth = you.dex * 3;

    if (you.skills[SK_STEALTH])
    {
        if (player_genus(GENPC_DRACONIAN))
            stealth += (you.skills[SK_STEALTH] * 12);
        else
        {
            switch (you.species)
            {
            case SP_TROLL:
            case SP_OGRE:
            case SP_OGRE_MAGE:
            case SP_CENTAUR:
                stealth += (you.skills[SK_STEALTH] * 9);
                break;
            case SP_MINOTAUR:
                stealth += (you.skills[SK_STEALTH] * 12);
                break;
            case SP_GNOME:
            case SP_HALFLING:
            case SP_KOBOLD:
            case SP_SPRIGGAN:
            case SP_NAGA:       // not small but very good at stealth
                stealth += (you.skills[SK_STEALTH] * 18);
                break;
            default:
                stealth += (you.skills[SK_STEALTH] * 15);
                break;
            }
        }
    }

    if (you.burden_state == BS_ENCUMBERED)
        stealth /= 2;
    else if (you.burden_state == BS_OVERLOADED)
        stealth /= 5;

    if (you.conf)
        stealth /= 3;

    const int arm   = you.equip[EQ_BODY_ARMOUR];
    const int cloak = you.equip[EQ_CLOAK];
    const int boots = you.equip[EQ_BOOTS];

    if (arm != -1 && !player_light_armour())
        stealth -= (item_mass( you.inv[arm] ) / 10);

    if (cloak != -1 && get_equip_race(you.inv[cloak]) == ISFLAG_ELVEN)
        stealth += 20;

    if (boots != -1)
    {
        if (get_armour_ego_type( you.inv[boots] ) == SPARM_STEALTH)
            stealth += 50;

        if (get_equip_race(you.inv[boots]) == ISFLAG_ELVEN)
            stealth += 20;
    }

    stealth += scan_randarts( RAP_STEALTH );

    if (player_is_levitating())
        stealth += 10;
    else if (player_in_water())
    {
        // Merfolk can sneak up on monsters underwater -- bwr
        if (you.species == SP_MERFOLK)
            stealth += 50;
        else
            stealth /= 2;       // splashy-splashy
    }
    else
        stealth -= you.mutation[MUT_HOOVES] * 10; // clippety-clop

    // Radiating silence is the negative complement of shouting all the
    // time... a sudden change from background noise to no noise is going
    // to clue anything in to the fact that something is very wrong...
    // a personal silence spell would naturally be different, but this
    // silence radiates for a distance and prevents monster spellcasting,
    // which pretty much gives away the stealth game.
    if (you.duration[DUR_SILENCE])
        stealth -= 50;

    if (stealth < 0)
        stealth = 0;

    return (stealth);
}                               // end check_stealth()

void ability_increase(void)
{
    unsigned char keyin;

    mpr("Your experience leads to an increase in your attributes!",
        MSGCH_INTRINSIC_GAIN);

    more();
    mesclr();

    mpr("Increase (S)trength, (I)ntelligence, or (D)exterity? ", MSGCH_PROMPT);

  get_key:
    keyin = getch();
    if (keyin == 0)
    {
        getch();
        goto get_key;
    }

    switch (keyin)
    {
    case 's':
    case 'S':
        modify_stat(STAT_STRENGTH, 1, false);
        return;

    case 'i':
    case 'I':
        modify_stat(STAT_INTELLIGENCE, 1, false);
        return;

    case 'd':
    case 'D':
        modify_stat(STAT_DEXTERITY, 1, false);
        return;
    }

    goto get_key;
/* this is an infinite loop because it is reasonable to assume that you're not going to want to leave it prematurely. */
}                               // end ability_increase()

void display_char_status(void)
{
    if (you.is_undead)
        mpr( "You are undead." );
    else if (you.deaths_door)
        mpr( "You are standing in death's doorway." );
    else
        mpr( "You are alive." );

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_SPIDER:
        mpr( "You are in spider-form." );
        break;
    case TRAN_BLADE_HANDS:
        mpr( "You have blades for hands." );
        break;
    case TRAN_STATUE:
        mpr( "You are a statue." );
        break;
    case TRAN_ICE_BEAST:
        mpr( "You are an ice creature." );
        break;
    case TRAN_DRAGON:
        mpr( "You are in dragon-form." );
        break;
    case TRAN_LICH:
        mpr( "You are in lich-form." );
        break;
    case TRAN_SERPENT_OF_HELL:
        mpr( "You are a huge demonic serpent." );
        break;
    case TRAN_AIR:
        mpr( "You are a cloud of diffuse gas." );
        break;
    }

    if (you.burden_state == BS_ENCUMBERED)
        mpr("You are burdened.");
    else if (you.burden_state == BS_OVERLOADED)
        mpr("You are overloaded with stuff.");

    if (you.duration[DUR_BREATH_WEAPON])
        mpr( "You are short of breath." );

    if (you.duration[DUR_REPEL_UNDEAD])
        mpr( "You have a holy aura protecting you from undead." );

    if (you.duration[DUR_LIQUID_FLAMES])
        mpr( "You are covered in liquid flames." );

    if (you.duration[DUR_ICY_ARMOUR])
        mpr( "You are protected by an icy shield." );

    if (you.duration[DUR_REPEL_MISSILES])
        mpr( "You are protected from missiles." );

    if (you.duration[DUR_DEFLECT_MISSILES])
        mpr( "You deflect missiles." );

    if (you.duration[DUR_PRAYER])
        mpr( "You are praying." );

    if (you.duration[DUR_REGENERATION])
        mpr( "You are regenerating." );

    if (you.duration[DUR_SWIFTNESS])
        mpr( "You can move swiftly." );

    if (you.duration[DUR_INSULATION])
        mpr( "You are insulated." );

    if (you.duration[DUR_STONEMAIL])
        mpr( "You are covered in scales of stone." );

    if (you.duration[DUR_CONTROLLED_FLIGHT])
        mpr( "You can control your flight." );

    if (you.duration[DUR_TELEPORT])
        mpr( "You are about to teleport." );

    if (you.duration[DUR_CONTROL_TELEPORT])
        mpr( "You can control teleportation." );

    if (you.duration[DUR_DEATH_CHANNEL])
        mpr( "You are channeling the dead." );

    if (you.duration[DUR_FORESCRY])     //jmf: added 19mar2000
        mpr( "You are forewarned." );

    if (you.duration[DUR_SILENCE])      //jmf: added 27mar2000
        mpr( "You radiate silence." );

    if (you.duration[DUR_STONESKIN])
        mpr( "Your skin is tough as stone." );

    if (you.duration[DUR_SEE_INVISIBLE])
        mpr( "You can see invisible." );

    if (you.invis)
        mpr( "You are invisible." );

    if (you.conf)
        mpr( "You are confused." );

    if (you.paralysis)
        mpr( "You are paralysed." );

    if (you.exhausted)
        mpr( "You are exhausted." );

    if (you.slow && you.haste)
        mpr( "You are under both slowing and hasting effects." );
    else if (you.slow)
        mpr( "Your actions are slowed." );
    else if (you.haste)
        mpr( "Your actions are hasted." );

    if (you.might)
        mpr( "You are mighty." );

    if (you.berserker)
        mpr( "You are possessed by a berserker rage." );

    if (player_is_levitating())
        mpr( "You are hovering above the floor." );

    if (you.poisoning)
    { 
        snprintf( info, INFO_SIZE, "You are %s poisoned.",
                  (you.poisoning > 10) ? "extremely" :
                  (you.poisoning > 5)  ? "very" :
                  (you.poisoning > 3)  ? "quite"
                                    : "mildly" );
        mpr(info);
    }

    if (you.disease)
    {
        snprintf( info, INFO_SIZE, "You are %sdiseased.",
                  (you.disease > 120) ? "badly " :
                  (you.disease >  40) ? ""
                                      : "mildly " );
        mpr(info);
    }

    if (you.rotting || you.species == SP_GHOUL)
    {
        // I apologize in advance for the horrendous ugliness about to
        // transpire.  Avert your eyes!
        snprintf( info, INFO_SIZE, "Your flesh is rotting%s",
                  (you.rotting > 15) ? " before your eyes!":
                  (you.rotting > 8)  ? " away quickly.":
                  (you.rotting > 4)  ? " badly."
             : ((you.species == SP_GHOUL && you.rotting > 0)
                        ? " faster than usual." : ".") );
        mpr(info);
    }

    contaminate_player( 0, true );

    if (you.confusing_touch)
    {
        snprintf( info, INFO_SIZE, "Your hands are glowing %s red.",
                  (you.confusing_touch > 40) ? "an extremely bright" :
                  (you.confusing_touch > 20) ? "bright"
                                             : "a soft" );
        mpr(info);
    }

    if (you.sure_blade)
    {
        snprintf( info, INFO_SIZE, "You have a %sbond with your blade.",
                  (you.sure_blade > 15) ? "strong " :
                  (you.sure_blade >  5) ? ""
                                        : "weak " );
        mpr(info);
    }

    int move_cost = (player_speed() * player_movement_speed()) / 10;
    if ( you.slow )
        move_cost *= 2;

    const bool water  = player_in_water();
    const bool swim   = player_is_swimming();

    const bool lev    = player_is_levitating();
    const bool fly    = (lev && you.duration[DUR_CONTROLLED_FLIGHT]);
    const bool swift  = (you.duration[DUR_SWIFTNESS] > 0);
    snprintf( info, INFO_SIZE,
              "Your %s speed is %s%s%s.",
              // order is important for these:
              (swim)    ? "swimming" :
              (water)   ? "wading" :
              (fly)     ? "flying" :
              (lev)     ? "levitating" 
                        : "movement", 

              (water && !swim)  ? "uncertain and " :
              (!water && swift) ? "aided by the wind" : "",

              (!water && swift) ? ((move_cost >= 10) ? ", but still "
                                                     :  " and ") 
                                : "",

              (move_cost <   8) ? "very quick" :
              (move_cost <  10) ? "quick" :
              (move_cost == 10) ? "average" :
              (move_cost <  13) ? "slow" 
                                : "very slow" );
    mpr(info);

    const int to_hit = calc_your_to_hit( false ) * 2;
    // Messages based largely on percentage chance of missing the 
    // average EV 10 humanoid, and very agile EV 30 (pretty much
    // max EV for monsters currently).
    //
    // "awkward"    - need lucky hit (less than EV)
    // "difficult"  - worse than 2 in 3
    // "hard"       - worse than fair chance
    snprintf( info, INFO_SIZE, 
              "%s given your current equipment.",
              (to_hit <   1) ? "You are completely incapable of fighting" :
              (to_hit <   5) ? "Hitting even clumsy monsters is extremely awkward" :
              (to_hit <  10) ? "Hitting average monsters is awkward" :
              (to_hit <  15) ? "Hitting average monsters is difficult" :
              (to_hit <  20) ? "Hitting average monsters is hard" :
              (to_hit <  30) ? "Very agile monsters are a bit awkward to hit" :
              (to_hit <  45) ? "Very agile monsters are a bit difficult to hit" :
              (to_hit <  60) ? "Very agile monsters are a bit hard to hit" :
              (to_hit < 100) ? "You feel comfortable with your ability to fight" 
              : "You feel confident with your ability to fight" );
    
#if DEBUG_DIAGNOSTICS
    char str_pass[INFO_SIZE];
    snprintf( str_pass, INFO_SIZE, " (%d)", to_hit );
    strncat( info, str_pass, INFO_SIZE );
#endif
    mpr(info);
    

    // magic resistance
    const int mr = player_res_magic();
    snprintf(info, INFO_SIZE, "You are %s resistant to magic.",
             (mr <  10) ? "not" :
             (mr <  30) ? "slightly" :
             (mr <  60) ? "somewhat" :
             (mr <  90) ? "quite" :
             (mr < 120) ? "very" :
             (mr < 140) ? "extremely" :
             "incredibly");
    mpr(info);

    // character evaluates their ability to sneak around: 
    const int ustealth = check_stealth();

    // XXX: made these values up, probably could be better.
    snprintf( info, INFO_SIZE, "You feel %sstealthy.",
              (ustealth <  10) ? "extremely un" :
              (ustealth <  20) ? "very un" :
              (ustealth <  30) ? "un" :
              (ustealth <  50) ? "fairly " :
              (ustealth <  80) ? "" :
              (ustealth < 120) ? "quite " :
              (ustealth < 160) ? "very " : 
              (ustealth < 200) ? "extremely " 
              : "incredibly " );

#if DEBUG_DIAGNOSTICS
    snprintf( str_pass, INFO_SIZE, " (%d)", ustealth );
    strncat( info, str_pass, INFO_SIZE );
#endif

    mpr( info );    
}                               // end display_char_status()

void redraw_skill(const char your_name[kNameLen], const char class_name[80])
{
    char print_it[80];

    memset( print_it, ' ', sizeof(print_it) );
    snprintf( print_it, sizeof(print_it), "%s the %s", your_name, class_name );

    int in_len = strlen( print_it );
    if (in_len > 40)
    {
        in_len -= 3;  // what we're getting back from removing "the"

        const int name_len = strlen(your_name);
        char name_buff[kNameLen];
        strncpy( name_buff, your_name, kNameLen );

        // squeeze name if required, the "- 8" is too not squeeze too much
        if (in_len > 40 && (name_len - 8) > (in_len - 40))
            name_buff[ name_len - (in_len - 40) - 1 ] = 0;
        else 
            name_buff[ kNameLen - 1 ] = 0;

        snprintf( print_it, sizeof(print_it), "%s, %s", name_buff, class_name );
    }

    for (int i = strlen(print_it); i < 41; i++)
        print_it[i] = ' ';

    print_it[40] = 0;

    gotoxy(40, 1);

    textcolor( LIGHTGREY );
    cprintf( "%s", print_it );
}                               // end redraw_skill()

// Does a case-sensitive lookup of the species name supplied.
int str_to_species(const std::string &species)
{
    if (species.empty())
        return SP_HUMAN;

    for (int i = SP_HUMAN; i < NUM_SPECIES; ++i)
    {
        if (species == species_name(i, 10))
            return (i);
    }

    for (int i = SP_HUMAN; i < NUM_SPECIES; ++i)
    {
        if (species == species_name(i, 1))
            return (i);
    }

    return (SP_HUMAN);
}

// Note that this function only has the one static buffer, so if you
// want to use the results, you'll want to make a copy.
char *species_name( int  speci, int level, bool genus, bool adj, bool cap )
// defaults:                               false       false     true
{
    static char species_buff[80];

    if (player_genus( GENPC_DRACONIAN, speci ))
    {
        if (adj || genus)  // adj doesn't care about exact species
            strcpy( species_buff, "Draconian" );
        else
        {
            // No longer have problems with ghosts here -- Sharp Aug2002
            if (level < 7)
                strcpy( species_buff, "Draconian" );
            else
            {
                switch (speci)
                {
                case SP_RED_DRACONIAN:
                    strcpy( species_buff, "Red Draconian" );
                    break;
                case SP_WHITE_DRACONIAN:
                    strcpy( species_buff, "White Draconian" );
                    break;
                case SP_GREEN_DRACONIAN:
                    strcpy( species_buff, "Green Draconian" );
                    break;
                case SP_GOLDEN_DRACONIAN:
                    strcpy( species_buff, "Yellow Draconian" );
                    break;
                case SP_GREY_DRACONIAN:
                    strcpy( species_buff, "Grey Draconian" );
                    break;
                case SP_BLACK_DRACONIAN:
                    strcpy( species_buff, "Black Draconian" );
                    break;
                case SP_PURPLE_DRACONIAN:
                    strcpy( species_buff, "Purple Draconian" );
                    break;
                case SP_MOTTLED_DRACONIAN:
                    strcpy( species_buff, "Mottled Draconian" );
                    break;
                case SP_PALE_DRACONIAN:
                    strcpy( species_buff, "Pale Draconian" );
                    break;
                case SP_UNK0_DRACONIAN:
                case SP_UNK1_DRACONIAN:
                case SP_BASE_DRACONIAN:
                default:
                    strcpy( species_buff, "Draconian" );
                    break;
                }
            }
        }
    }
    else if (player_genus( GENPC_ELVEN, speci ))
    {
        if (adj)  // doesn't care about species/genus
            strcpy( species_buff, "Elven" );
        else if (genus)
            strcpy( species_buff, "Elf" );
        else
        {
            switch (speci)
            {
            case SP_ELF:
            default:
                strcpy( species_buff, "Elf" );
                break;
            case SP_HIGH_ELF:
                strcpy( species_buff, "High Elf" );
                break;
            case SP_GREY_ELF:
                strcpy( species_buff, "Grey Elf" );
                break;
            case SP_DEEP_ELF:
                strcpy( species_buff, "Deep Elf" );
                break;
            case SP_SLUDGE_ELF:
                strcpy( species_buff, "Sludge Elf" );
                break;
            }
        }
    }
    else if (player_genus( GENPC_DWARVEN, speci ))
    {
        if (adj)  // doesn't care about species/genus
            strcpy( species_buff, "Dwarven" );
        else if (genus)
            strcpy( species_buff, "Dwarf" );
        else
        {
            switch (speci)
            {
            case SP_HILL_DWARF:
                strcpy( species_buff, "Hill Dwarf" );
                break;
            case SP_MOUNTAIN_DWARF:
                strcpy( species_buff, "Mountain Dwarf" );
                break;
            default:
                strcpy( species_buff, "Dwarf" );
                break;
            }
        }
    }
    else
    {
        switch (speci)
        {
        case SP_HUMAN:
            strcpy( species_buff, "Human" );
            break;
        case SP_HALFLING:
            strcpy( species_buff, "Halfling" );
            break;
        case SP_HILL_ORC:
            strcpy( species_buff, (adj) ? "Orcish" : (genus) ? "Orc" 
                                                             : "Hill Orc" );
            break;
        case SP_KOBOLD:
            strcpy( species_buff, "Kobold" );
            break;
        case SP_MUMMY:
            strcpy( species_buff, "Mummy" );
            break;
        case SP_NAGA:
            strcpy( species_buff, "Naga" );
            break;
        case SP_GNOME:
            strcpy( species_buff, (adj) ? "Gnomish" : "Gnome" );
            break;
        case SP_OGRE:
            strcpy( species_buff, (adj) ? "Ogreish" : "Ogre" );
            break;
        case SP_TROLL:
            strcpy( species_buff, (adj) ? "Trollish" : "Troll" );
            break;
        case SP_OGRE_MAGE:
            // We've previously declared that these are radically 
            // different from Ogres... so we're not going to 
            // refer to them as Ogres.  -- bwr
            strcpy( species_buff, "Ogre-Mage" );
            break;
        case SP_CENTAUR:
            strcpy( species_buff, "Centaur" );
            break;
        case SP_DEMIGOD:
            strcpy( species_buff, (adj) ? "Divine" : "Demigod" );
            break;
        case SP_SPRIGGAN:
            strcpy( species_buff, "Spriggan" );
            break;
        case SP_MINOTAUR:
            strcpy( species_buff, "Minotaur" );
            break;
        case SP_DEMONSPAWN:
            strcpy( species_buff, (adj) ? "Demonic" : "Demonspawn" );
            break;
        case SP_GHOUL:
            strcpy( species_buff, (adj) ? "Ghoulish" : "Ghoul" );
            break;
        case SP_KENKU:
            strcpy( species_buff, "Kenku" );
            break;
        case SP_MERFOLK:
            strcpy( species_buff, (adj) ? "Merfolkian" : "Merfolk" );
            break;
        default:
            strcpy( species_buff, (adj) ? "Yakish" : "Yak" );
            break;
        }
    }

    if (!cap)
        strlwr( species_buff );

    return (species_buff);
}                               // end species_name()

bool player_res_corrosion(bool calc_unid)
{
    return (player_equip(EQ_AMULET, AMU_RESIST_CORROSION, calc_unid)
            || player_equip_ego_type(EQ_CLOAK, SPARM_PRESERVATION));
}

bool player_item_conserve(bool calc_unid)
{
    return (player_equip(EQ_AMULET, AMU_CONSERVATION, calc_unid)
            || player_equip_ego_type(EQ_CLOAK, SPARM_PRESERVATION));
}

int player_mental_clarity(bool calc_unid)
{
    const int ret = 3 * player_equip(EQ_AMULET, AMU_CLARITY, calc_unid) 
                        + you.mutation[ MUT_CLARITY ];

    return ((ret > 3) ? 3 : ret);
}

bool wearing_amulet(char amulet, bool calc_unid)
{
    if (amulet == AMU_CONTROLLED_FLIGHT
        && (you.duration[DUR_CONTROLLED_FLIGHT]
            || player_genus(GENPC_DRACONIAN)
            || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON))
    {
        return true;
    }

    if (amulet == AMU_CLARITY && you.mutation[MUT_CLARITY])
        return true;

    if (amulet == AMU_RESIST_CORROSION || amulet == AMU_CONSERVATION)
    {
        // this is hackish {dlb}
        if (player_equip_ego_type( EQ_CLOAK, SPARM_PRESERVATION ))
            return true;
    }

    if (you.equip[EQ_AMULET] == -1)
        return false;

    if (you.inv[you.equip[EQ_AMULET]].sub_type == amulet
            && (calc_unid || 
                item_type_known(you.inv[you.equip[EQ_AMULET]])))
        return true;

    return false;
}                               // end wearing_amulet()

bool player_is_levitating(void)
{
    return you.is_levitating();
}

bool player_has_spell( int spell )
{
    return you.has_spell(spell);
}

int species_exp_mod(char species)
{

    if (player_genus(GENPC_DRACONIAN))
        return 14;
    else if (player_genus(GENPC_DWARVEN))
        return 13;
    {
        switch (species)
        {
        case SP_HUMAN:
        case SP_HALFLING:
        case SP_HILL_ORC:
        case SP_KOBOLD:
            return 10;
        case SP_GNOME:
            return 11;
        case SP_ELF:
        case SP_SLUDGE_ELF:
        case SP_NAGA:
        case SP_GHOUL:
        case SP_MERFOLK:
            return 12;
        case SP_SPRIGGAN:
        case SP_KENKU:
            return 13;
        case SP_GREY_ELF:
        case SP_DEEP_ELF:
        case SP_OGRE:
        case SP_CENTAUR:
        case SP_MINOTAUR:
        case SP_DEMONSPAWN:
            return 14;
        case SP_HIGH_ELF:
        case SP_MUMMY:
        case SP_TROLL:
        case SP_OGRE_MAGE:
            return 15;
        case SP_DEMIGOD:
            return 16;
        default:
            return 0;
        }
    }
}                               // end species_exp_mod()

unsigned long exp_needed(int lev)
{
    lev--;

    unsigned long level = 0;

#if 0
    case  1: level = 1;
    case  2: level = 10;
    case  3: level = 35;
    case  4: level = 70;
    case  5: level = 120;
    case  6: level = 250;
    case  7: level = 510;
    case  8: level = 900;
    case  9: level = 1700;
    case 10: level = 3500;
    case 11: level = 8000;
    case 12: level = 20000;

    default:                    //return 14000 * (lev - 11);
        level = 20000 * (lev - 11) + ((lev - 11) * (lev - 11) * (lev - 11)) * 130;
        break;
#endif

    // This is a better behaved function than the above.  The above looks 
    // really ugly when you consider the second derivative, its not smooth
    // and has a horrible bump at level 12 followed by comparitively easy
    // teen levels.  This tries to sort out those issues.
    //
    // Basic plan:
    // Section 1: levels  1- 5, second derivative goes 10-10-20-30.
    // Section 2: levels  6-13, second derivative is exponential/doubling.
    // Section 3: levels 14-27, second derivative is constant at 6000.
    //
    // Section three is constant so we end up with high levels at about 
    // their old values (level 27 at 850k), without delta2 ever decreasing.
    // The values that are considerably different (ie level 13 is now 29000, 
    // down from 41040 are because the second derivative goes from 9040 to 
    // 1430 at that point in the original, and then slowly builds back
    // up again).  This function smoothes out the old level 10-15 area 
    // considerably.

    // Here's a table:
    //
    // level      xp      delta   delta2
    // =====   =======    =====   ======
    //   1           0        0       0
    //   2          10       10      10
    //   3          30       20      10
    //   4          70       40      20
    //   5         140       70      30
    //   6         270      130      60
    //   7         520      250     120
    //   8        1010      490     240
    //   9        1980      970     480
    //  10        3910     1930     960
    //  11        7760     3850    1920
    //  12       15450     7690    3840
    //  13       29000    13550    5860
    //  14       48500    19500    5950
    //  15       74000    25500    6000
    //  16      105500    31500    6000
    //  17      143000    37500    6000
    //  18      186500    43500    6000
    //  19      236000    49500    6000
    //  20      291500    55500    6000
    //  21      353000    61500    6000
    //  22      420500    67500    6000
    //  23      494000    73500    6000
    //  24      573500    79500    6000
    //  25      659000    85500    6000
    //  26      750500    91500    6000
    //  27      848000    97500    6000


    switch (lev)
    {
    case 1:
        level = 1;
        break;
    case 2:
        level = 10;
        break;
    case 3:
        level = 30;
        break;
    case 4:
        level = 70;
        break;

    default:
        if (lev < 13)
        {
            lev -= 4;
            level = 10 + 10 * lev 
                       + 30 * (static_cast<int>(pow( 2.0, lev + 1 )));
        }
        else 
        {
            lev -= 12;
            level = 15500 + 10500 * lev + 3000 * lev * lev;
        }
        break;
    }

    return ((level - 1) * species_exp_mod(you.species) / 10);
}                               // end exp_needed()

// returns bonuses from rings of slaying, etc.
int slaying_bonus(char which_affected)
{
    int ret = 0;

    if (which_affected == PWPN_HIT)
    {
        ret += player_equip( EQ_RINGS_PLUS, RING_SLAYING );
        ret += scan_randarts(RAP_ACCURACY);
    }
    else if (which_affected == PWPN_DAMAGE)
    {
        ret += player_equip( EQ_RINGS_PLUS2, RING_SLAYING );
        ret += scan_randarts(RAP_DAMAGE);
    }

    return (ret);
}                               // end slaying_bonus()

/* Checks each equip slot for a randart, and adds up all of those with
   a given property. Slow if any randarts are worn, so avoid where possible. */
int scan_randarts(char which_property, bool calc_unid)
{
    int i = 0;
    int retval = 0;

    for (i = EQ_WEAPON; i < NUM_EQUIP; i++)
    {
        const int eq = you.equip[i];

        if (eq == -1)
            continue;

        // only weapons give their effects when in our hands
        if (i == EQ_WEAPON && you.inv[ eq ].base_type != OBJ_WEAPONS)
            continue;

        if (!is_random_artefact( you.inv[ eq ] ))
            continue;

        // Ignore unidentified items [TileCrawl dump enhancements].
        if (!item_ident(you.inv[ eq ], ISFLAG_KNOW_PROPERTIES) && 
                !calc_unid)
            continue;

        retval += randart_wpn_property( you.inv[ eq ], which_property );
    }

    return (retval);
}                               // end scan_randarts()

void modify_stat(unsigned char which_stat, char amount, bool suppress_msg)
{
    char *ptr_stat = NULL;
    char *ptr_stat_max = NULL;
    char *ptr_redraw = NULL;

    // sanity - is non-zero amount?
    if (amount == 0)
        return;

    // Stop delays if a stat drops.
    if (amount < 0)
        interrupt_activity( AI_STAT_CHANGE );

    if (!suppress_msg)
        strcpy(info, "You feel ");

    if (which_stat == STAT_RANDOM)
        which_stat = random2(NUM_STATS);

    switch (which_stat)
    {
    case STAT_STRENGTH:
        ptr_stat = &you.strength;
        ptr_stat_max = &you.max_strength;
        ptr_redraw = &you.redraw_strength;
        if (!suppress_msg)
            strcat(info, (amount > 0) ? "stronger." : "weaker.");
        break;

    case STAT_DEXTERITY:
        ptr_stat = &you.dex;
        ptr_stat_max = &you.max_dex;
        ptr_redraw = &you.redraw_dexterity;
        if (!suppress_msg)
            strcat(info, (amount > 0) ? "agile." : "clumsy.");
        break;

    case STAT_INTELLIGENCE:
        ptr_stat = &you.intel;
        ptr_stat_max = &you.max_intel;
        ptr_redraw = &you.redraw_intelligence;
        if (!suppress_msg)
            strcat(info, (amount > 0) ? "clever." : "stupid.");
        break;
    }

    if (!suppress_msg)
        mpr( info, (amount > 0) ? MSGCH_INTRINSIC_GAIN : MSGCH_WARN );

    *ptr_stat += amount;
    *ptr_stat_max += amount;
    *ptr_redraw = 1;

    if (ptr_stat == &you.strength)
        burden_change();

    return;
}                               // end modify_stat()

void dec_hp(int hp_loss, bool fatal, const char *aux)
{
    if (!fatal && you.hp < 1)
        you.hp = 1;

    if (!fatal && hp_loss >= you.hp)
        hp_loss = you.hp - 1;

    if (hp_loss < 1)
        return;

    // If it's not fatal, use ouch() so that notes can be taken. If it IS
    // fatal, somebody else is doing the bookkeeping, and we don't want to mess
    // with that.
    if (!fatal && aux)
        ouch(hp_loss, -1, KILLED_BY_SOMETHING, aux);
    else
        you.hp -= hp_loss;

    you.redraw_hit_points = 1;

    return;
}                               // end dec_hp()

void dec_mp(int mp_loss)
{
    if (mp_loss < 1)
        return;

    you.magic_points -= mp_loss;

    if (you.magic_points < 0)
        you.magic_points = 0;

    if (Options.magic_point_warning
        && you.magic_points < (you.max_magic_points 
                               * Options.magic_point_warning) / 100)
    {
        mpr( "* * * LOW MAGIC WARNING * * *", MSGCH_DANGER );
    }

    take_note(Note(NOTE_MP_CHANGE, you.magic_points, you.max_magic_points));
    you.redraw_magic_points = 1;

    return;
}                               // end dec_mp()

bool enough_hp(int minimum, bool suppress_msg)
{
    // We want to at least keep 1 HP -- bwr
    if (you.hp < minimum + 1)
    {
        if (!suppress_msg)
            mpr("You haven't enough vitality at the moment.");

        return false;
    }

    return true;
}                               // end enough_hp()

bool enough_mp(int minimum, bool suppress_msg)
{
    if (you.magic_points < minimum)
    {
        if (!suppress_msg)
            mpr("You haven't enough magic at the moment.");

        return false;
    }

    return true;
}                               // end enough_mp()

// Note that "max_too" refers to the base potential, the actual
// resulting max value is subject to penalties, bonuses, and scalings.
void inc_mp(int mp_gain, bool max_too)
{
    if (mp_gain < 1)
        return;

    you.magic_points += mp_gain;

    if (max_too)
        inc_max_mp( mp_gain );

    if (you.magic_points > you.max_magic_points)
        you.magic_points = you.max_magic_points;

    take_note(Note(NOTE_MP_CHANGE, you.magic_points, you.max_magic_points));
    you.redraw_magic_points = 1;

    return;
}                               // end inc_mp()

// Note that "max_too" refers to the base potential, the actual
// resulting max value is subject to penalties, bonuses, and scalings.
void inc_hp(int hp_gain, bool max_too)
{
    if (hp_gain < 1)
        return;

    you.hp += hp_gain;

    if (max_too)
        inc_max_hp( hp_gain );

    if (you.hp > you.hp_max)
        you.hp = you.hp_max;

    // to avoid message spam, no information when HP increases
    // take_note(Note(NOTE_HP_CHANGE, you.hp, you.hp_max));

    you.redraw_hit_points = 1;
}                               // end inc_hp()

void rot_hp( int hp_loss )
{
    you.base_hp -= hp_loss;
    calc_hp();

    you.redraw_hit_points = 1;
}

void unrot_hp( int hp_recovered )
{
    if (hp_recovered >= 5000 - you.base_hp)
        you.base_hp = 5000;
    else
        you.base_hp += hp_recovered;

    calc_hp();

    you.redraw_hit_points = 1;
}

int player_rotted( void )
{
    return (5000 - you.base_hp);
}

void rot_mp( int mp_loss )
{
    you.base_magic_points -= mp_loss;
    calc_mp();

    you.redraw_magic_points = 1;
}

void inc_max_hp( int hp_gain )
{
    you.base_hp2 += hp_gain;
    calc_hp();

    take_note(Note(NOTE_MAXHP_CHANGE, you.hp_max));
    you.redraw_hit_points = 1;
}

void dec_max_hp( int hp_loss )
{
    you.base_hp2 -= hp_loss;
    calc_hp();

    take_note(Note(NOTE_MAXHP_CHANGE, you.hp_max));
    you.redraw_hit_points = 1;
}

void inc_max_mp( int mp_gain )
{
    you.base_magic_points2 += mp_gain;
    calc_mp();

    take_note(Note(NOTE_MAXMP_CHANGE, you.max_magic_points));
    you.redraw_magic_points = 1;
}

void dec_max_mp( int mp_loss )
{
    you.base_magic_points2 -= mp_loss;
    calc_mp();

    take_note(Note(NOTE_MAXMP_CHANGE, you.max_magic_points));
    you.redraw_magic_points = 1;
}

// use of floor: false = hp max, true = hp min {dlb}
void deflate_hp(int new_level, bool floor)
{
    if (floor && you.hp < new_level)
        you.hp = new_level;
    else if (!floor && you.hp > new_level)
        you.hp = new_level;

    //    take_note(Note(NOTE_HP_CHANGE, you.hp, you.hp_max));
    // must remain outside conditional, given code usage {dlb}
    you.redraw_hit_points = 1;

    return;
}                               // end deflate_hp()

// Note that "max_too" refers to the base potential, the actual
// resulting max value is subject to penalties, bonuses, and scalings.
void set_hp(int new_amount, bool max_too)
{
    if (you.hp != new_amount)
        you.hp = new_amount;

    if (max_too && you.hp_max != new_amount)
    {
        you.base_hp2 = 5000 + new_amount;
        calc_hp();
    }

    if (you.hp > you.hp_max)
        you.hp = you.hp_max;

    //    take_note(Note(NOTE_HP_CHANGE, you.hp, you.hp_max));
    if ( max_too )
      take_note(Note(NOTE_MAXHP_CHANGE, you.hp_max));
    // must remain outside conditional, given code usage {dlb}
    you.redraw_hit_points = 1;

    return;
}                               // end set_hp()

// Note that "max_too" refers to the base potential, the actual
// resulting max value is subject to penalties, bonuses, and scalings.
void set_mp(int new_amount, bool max_too)
{
    if (you.magic_points != new_amount)
        you.magic_points = new_amount;

    if (max_too && you.max_magic_points != new_amount)
    {
        // note that this gets scaled down for values > 18
        you.base_magic_points2 = 5000 + new_amount;
        calc_mp();
    }

    if (you.magic_points > you.max_magic_points)
        you.magic_points = you.max_magic_points;
    
    take_note(Note(NOTE_MP_CHANGE, you.magic_points, you.max_magic_points));
    if ( max_too )
      take_note(Note(NOTE_MAXMP_CHANGE, you.max_magic_points));
    // must remain outside conditional, given code usage {dlb}
    you.redraw_magic_points = 1;

    return;
}                               // end set_mp()


static const char * Species_Abbrev_List[ NUM_SPECIES ] = 
    { "XX", "Hu", "El", "HE", "GE", "DE", "SE", "HD", "MD", "Ha",
      "HO", "Ko", "Mu", "Na", "Gn", "Og", "Tr", "OM", "Dr", "Dr", 
      "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", 
      "Ce", "DG", "Sp", "Mi", "DS", "Gh", "Ke", "Mf" };

int get_species_index_by_abbrev( const char *abbrev )
{
    int i;
    for (i = SP_HUMAN; i < NUM_SPECIES; i++)
    {
        if (tolower( abbrev[0] ) == tolower( Species_Abbrev_List[i][0] )
            && tolower( abbrev[1] ) == tolower( Species_Abbrev_List[i][1] ))
        {
            break;
        }
    }

    return ((i < NUM_SPECIES) ? i : -1);
}

int get_species_index_by_name( const char *name )
{
    int i;
    int sp = -1;

    char *ptr;
    char lowered_buff[80];

    strncpy( lowered_buff, name, sizeof( lowered_buff ) );
    strlwr( lowered_buff );

    for (i = SP_HUMAN; i < NUM_SPECIES; i++)
    {
        char *lowered_species = species_name( i, 0, false, false, false );
        ptr = strstr( lowered_species, lowered_buff );
        if (ptr != NULL)
        {
            sp = i;
            if (ptr == lowered_species)  // prefix takes preference
                break;
        }
    }

    return (sp);
}

const char *get_species_abbrev( int which_species )
{
    ASSERT( which_species > 0 && which_species < NUM_SPECIES );

    return (Species_Abbrev_List[ which_species ]);
}


static const char * Class_Abbrev_List[ NUM_JOBS ] = 
    { "Fi", "Wz", "Pr", "Th", "Gl", "Ne", "Pa", "As", "Be", "Hu", 
      "Cj", "En", "FE", "IE", "Su", "AE", "EE", "Cr", "DK", "VM", 
      "CK", "Tm", "He", "XX", "Re", "St", "Mo", "Wr", "Wn" };

static const char * Class_Name_List[ NUM_JOBS ] = 
    { "Fighter", "Wizard", "Priest", "Thief", "Gladiator", "Necromancer",
      "Paladin", "Assassin", "Berserker", "Hunter", "Conjurer", "Enchanter",
      "Fire Elementalist", "Ice Elementalist", "Summoner", "Air Elementalist",
      "Earth Elementalist", "Crusader", "Death Knight", "Venom Mage",
      "Chaos Knight", "Transmuter", "Healer", "Quitter", "Reaver", "Stalker",
      "Monk", "Warper", "Wanderer" };

int get_class_index_by_abbrev( const char *abbrev )
{
    int i;

    for (i = 0; i < NUM_JOBS; i++)
    {
        if (i == JOB_QUITTER)
            continue;

        if (tolower( abbrev[0] ) == tolower( Class_Abbrev_List[i][0] )
            && tolower( abbrev[1] ) == tolower( Class_Abbrev_List[i][1] ))
        {
            break;
        }
    }

    return ((i < NUM_JOBS) ? i : -1);
}

const char *get_class_abbrev( int which_job )
{
    ASSERT( which_job < NUM_JOBS && which_job != JOB_QUITTER );

    return (Class_Abbrev_List[ which_job ]);
}

int get_class_index_by_name( const char *name )
{
    int i;
    int cl = -1;

    char *ptr;
    char lowered_buff[80];
    char lowered_class[80];

    strncpy( lowered_buff, name, sizeof( lowered_buff ) );
    strlwr( lowered_buff );

    for (i = 0; i < NUM_JOBS; i++)
    {
        if (i == JOB_QUITTER)
            continue;

        strncpy( lowered_class, Class_Name_List[i], sizeof( lowered_class ) );
        strlwr( lowered_class );

        ptr = strstr( lowered_class, lowered_buff );
        if (ptr != NULL)
        {
            cl = i;
            if (ptr == lowered_class)  // prefix takes preference
                break;
        }
    }

    return (cl);
}

const char *get_class_name( int which_job ) 
{
    ASSERT( which_job < NUM_JOBS && which_job != JOB_QUITTER );

    return (Class_Name_List[ which_job ]);
}

void contaminate_player(int change, bool statusOnly)
{
    // get current contamination level
    int old_level;
    int new_level;


#if DEBUG_DIAGNOSTICS
    if (change > 0 || (change < 0 && you.magic_contamination))
    {
        snprintf( info, INFO_SIZE, "change: %d  radiation: %d", 
                 change, change + you.magic_contamination );

        mpr( info, MSGCH_DIAGNOSTICS );
    }
#endif

    old_level = (you.magic_contamination > 60)?(you.magic_contamination / 20 + 2) :
                (you.magic_contamination > 40)?4 :
                (you.magic_contamination > 25)?3 :
                (you.magic_contamination > 15)?2 :
                (you.magic_contamination > 5)?1  : 0;

    // make the change
    if (change + you.magic_contamination < 0)
        you.magic_contamination = 0;
    else
    {
        if (change + you.magic_contamination > 250)
            you.magic_contamination = 250;
        else
            you.magic_contamination += change;
    }

    // figure out new level
    new_level = (you.magic_contamination > 60)?(you.magic_contamination / 20 + 2) :
                (you.magic_contamination > 40)?4 :
                (you.magic_contamination > 25)?3 :
                (you.magic_contamination > 15)?2 :
                (you.magic_contamination > 5)?1  : 0;

    if (statusOnly)
    {
        if (new_level > 0)
        {
            if (new_level > 3)
            {
                strcpy(info, (new_level == 4) ?
                    "Your entire body has taken on an eerie glow!" :
                    "You are engulfed in a nimbus of crackling magics!");
            }
            else
            {
                snprintf( info, INFO_SIZE, "You are %s with residual magics%c",
                    (new_level == 3) ? "practically glowing" :
                    (new_level == 2) ? "heavily infused"
                                     : "contaminated",
                    (new_level == 3) ? '!' : '.');
            }

            mpr(info);
        }
        return;
    }

    if (new_level == old_level)
        return;

    snprintf( info, INFO_SIZE, "You feel %s contaminated with magical energies.", 
              (change < 0) ? "less" : "more" );

    mpr( info, (change > 0) ? MSGCH_WARN : MSGCH_RECOVERY );
}

void poison_player( int amount, bool force )
{
    if ((!force && player_res_poison()) || amount <= 0)
        return;

    const int old_value = you.poisoning;
    you.poisoning += amount;

    if (you.poisoning > 40)
        you.poisoning = 40;

    if (you.poisoning > old_value)
    {
        snprintf( info, INFO_SIZE, "You are %spoisoned.",
                  (old_value > 0) ? "more " : "" );

        // XXX: which message channel for this message?
        mpr( info );
        learned_something_new(TUT_YOU_POISON);
    }
}

void reduce_poison_player( int amount )
{
    if (you.poisoning == 0 || amount <= 0)
        return;

    you.poisoning -= amount;

    if (you.poisoning <= 0)
    {
        you.poisoning = 0;
        mpr( "You feel better.", MSGCH_RECOVERY );
    }
    else
    {
        mpr( "You feel a little better.", MSGCH_RECOVERY );
    }
}

void confuse_player( int amount, bool resistable )
{
    if (amount <= 0)
        return;

    if (resistable && wearing_amulet(AMU_CLARITY))
    {
        mpr( "You feel momentarily confused." );
        return;
    }

    const int old_value = you.conf;
    you.conf += amount;

    if (you.conf > 40)
        you.conf = 40;

    if (you.conf > old_value)
    {
        snprintf( info, INFO_SIZE, "You are %sconfused.", 
                  (old_value > 0) ? "more " : "" );

        // XXX: which message channel for this message?
        mpr( info );
        learned_something_new(TUT_YOU_ENCHANTED);
    }
}

void reduce_confuse_player( int amount )
{
    if (you.conf == 0 || amount <= 0)
        return;

    you.conf -= amount;

    if (you.conf <= 0)
    {
        you.conf = 0;
        mpr( "You feel less confused." );
    }
}

void slow_player( int amount )
{
    if (amount <= 0)
        return;

    if (wearing_amulet( AMU_RESIST_SLOW ))
        mpr("You feel momentarily lethargic.");
    else if (you.slow >= 100)
        mpr( "You already are as slow as you could be." );
    else
    {   
        if (you.slow == 0)
            mpr( "You feel yourself slow down." );
        else
            mpr( "You feel as though you will be slow longer." );

        you.slow += amount;

        if (you.slow > 100)
            you.slow = 100;
        learned_something_new(TUT_YOU_ENCHANTED);
    }
}

void dec_slow_player( void )
{
    if (you.slow > 1)
    {
        // BCR - Amulet of resist slow affects slow counter
        if (wearing_amulet(AMU_RESIST_SLOW))
        {
            you.slow -= 5;
            if (you.slow < 1)
                you.slow = 1;
        }
        else
            you.slow--;
    }
    else if (you.slow == 1)
    {
        mpr("You feel yourself speed up.", MSGCH_DURATION);
        you.slow = 0;
    }
}

void haste_player( int amount )
{
    bool amu_eff = wearing_amulet( AMU_RESIST_SLOW );

    if (amount <= 0)
        return;

    if (amu_eff)
        mpr( "Your amulet glows brightly." );

    if (you.haste == 0)
        mpr( "You feel yourself speed up." );
    else if (you.haste > 80 + 20 * amu_eff)
        mpr( "You already have as much speed as you can handle." );
    else
    {
        mpr( "You feel as though your hastened speed will last longer." );
        contaminate_player(1);
    }

    you.haste += amount;

    if (you.haste > 80 + 20 * amu_eff)
        you.haste = 80 + 20 * amu_eff;

    did_god_conduct( DID_STIMULANTS, 4 + random2(4) );
}

void dec_haste_player( void )
{
    if (you.haste > 1)
    {
        // BCR - Amulet of resist slow affects haste counter
        if (!wearing_amulet(AMU_RESIST_SLOW) || coinflip())
            you.haste--;

        if (you.haste == 6)
        {
            mpr( "Your extra speed is starting to run out.", MSGCH_DURATION );
            if (coinflip())
                you.haste--;
        }
    }
    else if (you.haste == 1)
    {
        mpr( "You feel yourself slow down.", MSGCH_DURATION );
        you.haste = 0;
    }
}

void disease_player( int amount )
{
    if (you.is_undead || amount <= 0)
        return;

    mpr( "You feel ill." );

    const int tmp = you.disease + amount;
    you.disease = (tmp > 210) ? 210 : tmp;
    learned_something_new(TUT_YOU_SICK);    
}

void dec_disease_player( void )
{
    if (you.disease > 0)
    {       
        you.disease--;
                
        if (you.disease > 5
            && (you.species == SP_KOBOLD 
                || you.duration[ DUR_REGENERATION ]
                || you.mutation[ MUT_REGENERATION ] == 3))
        {
            you.disease -= 2;
        }
                 
        if (!you.disease)
            mpr("You feel your health improve.", MSGCH_RECOVERY);
    }
}

void rot_player( int amount )
{
    if (amount <= 0)
        return;

    if (you.rotting < 40)
    {
        // Either this, or the actual rotting message should probably
        // be changed so that they're easier to tell apart. -- bwr
        snprintf( info, INFO_SIZE, "You feel your flesh %s away!",
                  (you.rotting) ? "rotting" : "start to rot" );
        mpr( info, MSGCH_WARN );

        you.rotting += amount;
    }
}

//////////////////////////////////////////////////////////////////////////////
// actor

actor::~actor()
{
}

//////////////////////////////////////////////////////////////////////////////
// player

player::player()
{
    init();
}

// player struct initialization
void player::init()
{
    birth_time = time( NULL );
    real_time = 0;
    num_turns = 0;

#ifdef WIZARD
    wizard = (Options.wiz_mode == WIZ_YES) ? true : false;
#else
    wizard = false;
#endif

    your_name[0] = 0;

    banished = false;
    banished_by.clear();
    
    just_autoprayed = false;
    berserk_penalty = 0;
    berserker = 0;
    conf = 0;
    confusing_touch = 0;
    deaths_door = 0;
    disease = 0;
    elapsed_time = 0;
    exhausted = 0;
    haste = 0;
    invis = 0;
    levitation = 0;
    might = 0;
    paralysis = 0;
    poisoning = 0;
    rotting = 0;
    fire_shield = 0;
    slow = 0;
    special_wield = SPWLD_NONE;
    sure_blade = 0;
    synch_time = 0;

    base_hp = 5000;
    base_hp2 = 5000;
    base_magic_points = 5000;
    base_magic_points2 = 5000;

    magic_points_regeneration = 0;
    strength = 0;
    max_strength = 0;
    intel = 0;
    max_intel = 0;
    dex = 0;
    max_dex = 0;
    experience = 0;
    experience_level = 1;
    max_level = 1;
    char_class = JOB_UNKNOWN;

    hunger = 6000;
    hunger_state = HS_SATIATED;

    gold = 0;
    // speed = 10;             // 0.75;  // unused

    burden = 0;
    burden_state = BS_UNENCUMBERED;

    spell_no = 0;

    your_level = 0;
    level_type = LEVEL_DUNGEON;
    where_are_you = BRANCH_MAIN_DUNGEON;
    char_direction = DIR_DESCENDING;

    prev_targ = MHITNOT;
    pet_target = MHITNOT;

    x_pos = 0;
    y_pos = 0;

    running.clear();
    travel_x = 0;
    travel_y = 0;

    religion = GOD_NO_GOD;
    piety = 0;

    gift_timeout = 0;

    for (int i = 0; i < MAX_NUM_GODS; i++)
    {
        penance[i] = 0;
        worshipped[i] = 0;
        num_gifts[i] = 0;
    }

    for (int i = EQ_WEAPON; i < NUM_EQUIP; i++)
        equip[i] = -1;

    for (int i = 0; i < 25; i++)
        spells[i] = SPELL_NO_SPELL;

    for (int i = 0; i < 52; i++)
    {
        spell_letter_table[i] = -1;
        ability_letter_table[i] = ABIL_NON_ABILITY;
    }

    for (int i = 0; i < 100; i++)
        mutation[i] = 0;

    for (int i = 0; i < 100; i++)
        demon_pow[i] = 0;

    for (int i = 0; i < 50; i++)
        had_book[i] = 0;

    for (int i = 0; i < 50; i++)
        unique_items[i] = UNIQ_NOT_EXISTS;

    for (int i = 0; i < NO_UNRANDARTS; i++)
        set_unrandart_exist(i, 0);

    for (int i = 0; i < 50; i++)
    {
        skills[i] = 0;
        skill_points[i] = 0;
        skill_order[i] = MAX_SKILL_ORDER;
        practise_skill[i] = 1;
    }

    skill_cost_level = 1;
    total_skill_points = 0;

    for (int i = 0; i < 30; i++)
        attribute[i] = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        inv[i].quantity = 0;
        inv[i].base_type = OBJ_WEAPONS;
        inv[i].sub_type = WPN_CLUB;
        inv[i].plus = 0;
        inv[i].plus2 = 0;
        inv[i].special = 0;
        inv[i].colour = 0;
        set_ident_flags( inv[i], ISFLAG_IDENT_MASK );

        inv[i].x = -1;
        inv[i].y = -1;
        inv[i].link = i;
    }

    for (int i = 0; i < NUM_DURATIONS; i++)
        duration[i] = 0;

    exp_available = 25;
}

bool player::is_valid() const
{
    // Check if there's a name.
    return (your_name[0] != 0);
}

std::string player::short_desc() const
{
    std::string desc;
    desc += your_name;
    desc += ", a level ";

    char st_prn[20];
    itoa(experience_level, st_prn, 10);
    desc += st_prn;

    desc += " ";
    desc += species_name(species, experience_level);
    desc += " ";
    desc += class_name;

    return (desc);
}

bool player::operator < (const player &p) const
{
    return (experience > p.experience
        || (experience == p.experience &&
                stricmp(your_name, p.your_name) < 0));
}

coord_def player::pos() const
{
    return coord_def(x_pos, y_pos);
}

bool player::is_levitating() const
{
    return (attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON || levitation);
}

bool player::in_water() const
{
    return !is_levitating()
        && grid_is_water(grd[you.x_pos][you.y_pos]);
}

bool player::can_swim() const
{
    return (species == SP_MERFOLK);
}

bool player::swimming() const
{
    return in_water() && can_swim();
}

bool player::has_spell(int spell) const
{
    for (int i = 0; i < 25; i++)
    {
        if (spells[i] == spell)
            return (true);
    }

    return (false);
}

bool player::floundering() const
{
    return in_water() && !can_swim();
}

size_type player::body_size(int psize, bool base) const
{
    size_type ret = (base) ? SIZE_CHARACTER : transform_size( psize );

    if (ret == SIZE_CHARACTER)
    {
        // transformation has size of character's species:
        switch (species)
        {
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_TROLL:
            ret = SIZE_LARGE;
            break;

        case SP_NAGA:
            // Most of their body is on the ground giving them a low profile.
            if (psize == PSIZE_TORSO || psize == PSIZE_PROFILE)
                ret = SIZE_MEDIUM;
            else
                ret = SIZE_BIG;
            break;

        case SP_CENTAUR:
            ret = (psize == PSIZE_TORSO) ? SIZE_MEDIUM : SIZE_BIG;
            break;

        case SP_SPRIGGAN:
            ret = SIZE_LITTLE;
            break;

        case SP_HALFLING:
        case SP_GNOME:
        case SP_KOBOLD:
            ret = SIZE_SMALL;
            break;

        default:
            ret = SIZE_MEDIUM;
            break;
        }
    }

    return (ret);
}

bool player::cannot_speak() const
{
    if (silenced(x_pos, y_pos))
        return (true);

    // No transform that prevents the player from speaking yet.
    return (false);
}

std::string player::shout_verb() const
{
    const int transform = attribute[ATTR_TRANSFORMATION];
    switch (transform)
    {
    case TRAN_DRAGON:
        return "roar";
    case TRAN_SPIDER:
        return "hiss";
    default:
        return "yell";
    }
}

int player::damage_type(int)
{
    const int wpn = equip[ EQ_WEAPON ];

    if (wpn != -1)
    {
        return (get_vorpal_type(inv[wpn]));
    }
    else if (equip[EQ_GLOVES] == -1 &&
             attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
    {
        return (DVORP_SLICING);
    }
    else if (has_usable_claws())
    {
        return (DVORP_CLAWING);
    }

    return (DVORP_CRUSHING);
}

int player::damage_brand(int)
{
    int ret = SPWPN_NORMAL;
    const int wpn = equip[ EQ_WEAPON ];

    if (wpn != -1) 
    {
        if ( !is_range_weapon(inv[wpn]) )
            ret = get_weapon_brand( inv[wpn] );
    }
    else if (confusing_touch)
        ret = SPWPN_CONFUSE;
    else if (mutation[MUT_DRAIN_LIFE])
        ret = SPWPN_DRAINING;
    else
    {
        switch (attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_SPIDER:
            ret = SPWPN_VENOM;
            break;

        case TRAN_ICE_BEAST:
            ret = SPWPN_FREEZING;
            break;

        case TRAN_LICH:
            ret = SPWPN_DRAINING;
            break;

        default:
            break;
        }
    }

    return (ret);
}

item_def *player::slot_item(equipment_type eq)
{
    ASSERT(eq >= EQ_WEAPON && eq <= EQ_AMULET);

    const int item = equip[eq];
    return (item == -1? NULL : &inv[item]);
}

// Returns the item in the player's weapon slot.
item_def *player::weapon(int /* which_attack */)
{
    return slot_item(EQ_WEAPON);
}

item_def *player::shield()
{
    return slot_item(EQ_SHIELD);
}

std::string player::name(description_level_type type) const
{
    return (pronoun_you(type));
}

std::string player::pronoun(pronoun_type pro) const
{
    switch (pro)
    {
    default:
    case PRONOUN_CAP:   return "You";
    case PRONOUN_NOCAP: return "you";
    case PRONOUN_CAP_POSSESSIVE: return "Your";
    case PRONOUN_NOCAP_POSSESSIVE: return "your";
    case PRONOUN_REFLEXIVE: return "yourself";
    }
}

std::string player::conj_verb(const std::string &verb) const
{
    return (verb);
}

int player::id() const
{
    return (-1);
}

bool player::alive() const
{
    // Simplistic, but if the player dies the game is over anyway, so
    // nobody can ask further questions.
    return (true);
}

bool player::fumbles_attack(bool verbose)
{
    // fumbling in shallow water <early return>:
    if (floundering())
    {
        if (random2(dex) < 4 || one_chance_in(5))
        {
            if (verbose)
                mpr("Unstable footing causes you to fumble your attack.");
            return (true);
        }
    }
    return (false);
}

bool player::cannot_fight() const
{
    return (false);
}

void player::attacking(actor *other)
{
    if (other && other->atype() == ACT_MONSTER)
    {
        const monsters *mons = dynamic_cast<monsters*>(other);
        if (mons_friendly(mons))
            did_god_conduct(DID_ATTACK_FRIEND, 5);
        else
            pet_target = monster_index(mons);
    }

    if (mutation[MUT_BERSERK] &&
        (random2(100) < (mutation[MUT_BERSERK] * 10) - 5))
    {
        go_berserk(false);
    }
}

void player::go_berserk(bool intentional)
{
    ::go_berserk(intentional);
}

void player::god_conduct(int thing_done, int level)
{
    ::did_god_conduct(thing_done, level);
}

void player::banish(const std::string &who)
{
    banished    = true;
    banished_by = who;
}

void player::make_hungry(int hunger_increase, bool silent)
{
    if (hunger_increase > 0)
        ::make_hungry(hunger_increase, silent);
    else if (hunger_increase < 0)
        ::lessen_hunger(-hunger_increase, silent);
}

int player::holy_aura() const
{
    return (duration[DUR_REPEL_UNDEAD]? piety : 0);
}

int player::warding() const
{
    if (wearing_amulet(AMU_WARDING))
        return (30);

    if (religion == GOD_VEHUMET && duration[DUR_PRAYER]
        && !player_under_penance() && piety >= piety_breakpoint(2))
    {
        // Clamp piety at 160 and scale that down to a max of 30.
        const int wardpiety = piety > 160? 160 : piety;
        return (wardpiety * 3 / 16);
    }

    return (0);
}

bool player::paralysed() const
{
    return (paralysis);
}

bool player::confused() const
{
    return (conf);
}

int player::shield_block_penalty() const
{
    return (5 * shield_blocks * shield_blocks);
}

int player::shield_bonus() const
{
    const item_def *sh = const_cast<player*>(this)->shield();
    if (!sh)
        return (0);

    const int stat =
        sh->sub_type == ARM_BUCKLER?      dex :
        sh->sub_type == ARM_LARGE_SHIELD? (3 * strength + dex) / 4:
        (dex + strength) / 2;
    
    return random2(player_shield_class()) 
        + (random2(stat) / 4)
        + (random2(skill_bump(SK_SHIELDS)) / 4)
        - 1;
}

int player::shield_bypass_ability(int tohit) const
{
    return (10 + tohit * 2);
}

void player::shield_block_succeeded()
{
    shield_blocks++;
    if (coinflip())
        exercise(SK_SHIELDS, 1);
}

bool player::wearing_light_armour(bool with_skill) const
{
    return (player_light_armour(with_skill));
}

void player::exercise(skill_type sk, int qty)
{
    ::exercise(sk, qty);
}

int player::skill(skill_type sk, bool bump) const
{
    return (bump? skill_bump(sk) : skills[sk]);
}

int player::armour_class() const
{
    return (player_AC());
}

int player::melee_evasion(const actor *act) const
{
    return (player_evasion()
            - (act->visible()? 0 : 10)
            - (you_are_delayed()? 5 : 0));
}

void player::heal(int amount, bool max_too)
{
    ::inc_hp(amount, max_too);
}

int player::holiness() const
{
    if (is_undead)
        return (MH_UNDEAD);

    if (species == SP_DEMONSPAWN)
        return (MH_DEMONIC);

    return (MH_NATURAL);
}

int player::res_fire() const
{
    return (player_res_fire());
}

int player::res_cold() const
{
    return (player_res_cold());
}

int player::res_elec() const
{
    return (player_res_electricity());
}

int player::res_poison() const
{
    return (player_res_poison());
}

int player::res_negative_energy() const
{
    return (player_prot_life());
}

bool player::levitates() const
{
    return (player_is_levitating());
}

int player::mons_species() const
{
    switch (species)
    {
    case SP_HILL_ORC:
        return (MONS_ORC);
    case SP_ELF: case SP_HIGH_ELF: case SP_GREY_ELF:
    case SP_DEEP_ELF: case SP_SLUDGE_ELF:
        return (MONS_ELF);
    default:
        return (MONS_HUMAN);
    }
}

void player::poison(actor*, int amount)
{
    ::poison_player(amount);
}

void player::expose_to_element(beam_type element, int st)
{
    ::expose_player_to_element(element, st);
}

void player::blink()
{
    random_blink(true);
}

void player::teleport(bool now, bool abyss_shift)
{
    if (now)
        you_teleport2(true, abyss_shift);
    else
        you_teleport();
}

void player::hurt(actor *agent, int amount)
{
    if (agent->atype() == ACT_MONSTER)
        ouch(amount, monster_index( dynamic_cast<monsters*>(agent) ),
             KILLED_BY_MONSTER);
    else
    {
        // Should never happen!
        ASSERT(false);
        ouch(amount, 0, KILLED_BY_SOMETHING);
    }

    if (religion == GOD_XOM && hp <= hp_max / 3
        && one_chance_in(10))
    {
        Xom_acts(true, experience_level, false);
    }
}

void player::drain_stat(int stat, int amount)
{
    lose_stat(stat, amount);
}

void player::rot(actor *who, int rotlevel, int immed_rot)
{
    if (is_undead)
        return;

    if (rotlevel)
        rot_player( rotlevel );
    
    if (immed_rot)
        rot_hp(immed_rot);

    if (rotlevel && one_chance_in(4))
        disease_player( 50 + random2(100) );
}

void player::confuse(int str)
{
    confuse_player(str);
}

void player::paralyse(int str)
{
    mprf( "You %s the ability to move!",
          (paralysis) ? "still haven't" : "suddenly lose" );
    
    if (str > paralysis)
        paralysis = str;
    
    if (paralysis > 13)
        paralysis = 13;
}

void player::slow_down(int str)
{
    ::slow_player( str );
}

bool player::has_usable_claws() const
{
    return (equip[EQ_GLOVES] == -1 &&
            (attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
             || mutation[MUT_CLAWS] 
             || species == SP_TROLL
             || species == SP_GHOUL));
}

god_type player::deity() const
{
    return (religion);
}

kill_category player::kill_alignment() const
{
    return (KC_YOU);
}
