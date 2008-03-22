/*
 *  File:       spells3.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <2>     9/11/99        LRH    Teleportation takes longer in the Abyss
 *      <2>     8/05/99        BWR    Added allow_control_teleport
 *      <1>     -/--/--        LRH    Created
 */

#include "AppHdr.h"
#include "spells3.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

#include "externs.h"

#include "abyss.h"
#include "beam.h"
#include "branch.h"
#include "cloud.h"
#include "direct.h"
#include "debug.h"
#include "delay.h"
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "it_use2.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "mon-pick.h"
#include "monstuff.h"
#include "mon-util.h"
#include "place.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "spells1.h"
#include "spells4.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "traps.h"
#include "view.h"

static bool monster_on_level(int monster);

bool cast_selective_amnesia(bool force)
{
    char ep_gain = 0;
    unsigned char keyin = 0;

    if (you.spell_no == 0)
        mpr("You don't know any spells.");      // re: sif muna {dlb}
    else
    {
        // query - conditional ordering is important {dlb}:
        for (;;)
        {
            mpr( "Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT );

            keyin = get_ch();

            if (keyin == ESCAPE)
                return (false);        // early return {dlb}

            if (keyin == '?' || keyin == '*')
            {
                // this reassignment is "key" {dlb}
                keyin = (unsigned char) list_spells();

                redraw_screen();
            }

            if (!isalpha( keyin ))
                mesclr( true );
            else
                break;
        }

        // actual handling begins here {dlb}:
        const spell_type spell = get_spell_by_letter( keyin );
        const int slot  = get_spell_slot_by_letter( keyin );

        if (spell == SPELL_NO_SPELL)
            mpr( "You don't know that spell." );
        else
        {
            if (!force
                 && (you.religion != GOD_SIF_MUNA
                     && random2(you.skills[SK_SPELLCASTING])
                         < random2(spell_difficulty( spell ))))
            {
                mpr("Oops! This spell sure is a blunt instrument.");
                forget_map(20 + random2(50));
            }
            else
            {
                ep_gain = spell_mana( spell );
                del_spell_from_memory_by_slot( slot );

                if (ep_gain > 0)
                {
                    inc_mp(ep_gain, false);
                    mpr( "The spell releases its latent energy back to you as "
                         "it unravels." );
                }
            }
        }
    }

    return (true);
}                               // end cast_selective_amnesia()

bool remove_curse(bool suppress_msg)
{
    int loopy = 0;              // general purpose loop variable {dlb}
    bool success = false;       // whether or not curse(s) removed {dlb}

    // special "wield slot" case - see if you can figure out why {dlb}:
    // because only cursed weapons in hand only count as cursed -- bwr
    if (you.equip[EQ_WEAPON] != -1
                && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_WEAPONS)
    {
        if (item_cursed( you.inv[you.equip[EQ_WEAPON]] ))
        {
            do_uncurse_item( you.inv[you.equip[EQ_WEAPON]] );
            success = true;
            you.wield_change = true;
        }
    }

    // everything else uses the same paradigm - are we certain?
    // what of artefact rings and amulets? {dlb}:
    for (loopy = EQ_CLOAK; loopy < NUM_EQUIP; loopy++)
    {
        if (you.equip[loopy] != -1 && item_cursed(you.inv[you.equip[loopy]]))
        {
            do_uncurse_item( you.inv[you.equip[loopy]] );
            success = true;
        }
    }

    // messaging output {dlb}:
    if (!suppress_msg)
    {
        if (success)
            mpr("You feel as if something is helping you.");
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return (success);
}                               // end remove_curse()

bool detect_curse(bool suppress_msg)
{
    int loopy = 0;              // general purpose loop variable {dlb}
    bool success = false;       // whether or not any curses found {dlb}

    for (loopy = 0; loopy < ENDOFPACK; loopy++)
    {
        if (you.inv[loopy].quantity
            && (you.inv[loopy].base_type == OBJ_WEAPONS
                || you.inv[loopy].base_type == OBJ_ARMOUR
                || you.inv[loopy].base_type == OBJ_JEWELLERY))
        {
            if (!item_ident( you.inv[loopy], ISFLAG_KNOW_CURSE ))
                success = true;

            set_ident_flags( you.inv[loopy], ISFLAG_KNOW_CURSE );
        }
    }

    // messaging output {dlb}:
    if (!suppress_msg)
    {
        if (success)
            mpr("You sense the presence of curses on your possessions.");
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return (success);
}                               // end detect_curse()

int cast_smiting(int power, dist &beam)
{
    bool success = false;
    monsters *monster = 0;       // NULL {dlb}

    if (mgrd[beam.tx][beam.ty] == NON_MONSTER
        || beam.isMe)
    {
        canned_msg(MSG_SPELL_FIZZLES);
    }
    else
    {
        monster = &menv[mgrd[beam.tx][beam.ty]];

        mprf("You smite %s!", monster->name(DESC_NOCAP_THE).c_str());

        // Maxes out at around 40 damage at 27 Invocations, which is plenty
        // in my book (the old max damage was around 70, which seems excessive)
        hurt_monster(monster, 7 + (random2(power) * 33 / 191));

        if (mons_friendly(monster))
            did_god_conduct(DID_ATTACK_FRIEND, 5, true, monster);

        behaviour_event( monster, ME_ANNOY, MHITYOU );

        if (monster->hit_points < 1)
            monster_die(monster, KILL_YOU, 0);
        else
        {
            const monsters *mons = static_cast<const monsters*>(monster);
            print_wounds(mons);
        }

        success = true;
    }

    return (success);
}                               // end cast_smiting()

int airstrike(int power, dist &beam)
{
    bool success = false;
    struct monsters *monster = 0;       // NULL {dlb}
    int hurted = 0;

    if (mgrd[beam.tx][beam.ty] == NON_MONSTER
        || beam.isMe)
    {
        canned_msg(MSG_SPELL_FIZZLES);
    }
    else
    {
        monster = &menv[mgrd[beam.tx][beam.ty]];

        mprf("The air twists around and strikes %s!",
             monster->name(DESC_NOCAP_THE).c_str());

        hurted = 8 + random2( random2(4) + (random2(power) / 6)
                              + (random2(power) / 7) );
        
        if ( mons_flies(monster) )
        {
            hurted *= 3;
            hurted /= 2;
        }

        hurted -= random2(1 + monster->ac);
        
        if (hurted < 0)
            hurted = 0;
        else
        {
            hurt_monster(monster, hurted);

            if (mons_friendly(monster))
                did_god_conduct(DID_ATTACK_FRIEND, 5, true, monster);

            behaviour_event(monster, ME_ANNOY, MHITYOU, you.x_pos, you.y_pos);

            if (monster->hit_points < 1)
                monster_die(monster, KILL_YOU, 0);
            else
                print_wounds(monster);
        }

        success = true;
    }

    return (success);
}                               // end airstrike()

int cast_bone_shards(int power, bolt &beam)
{
    bool success = false;

    if (you.equip[EQ_WEAPON] == -1
                    || you.inv[you.equip[EQ_WEAPON]].base_type != OBJ_CORPSES)
    {
        canned_msg(MSG_SPELL_FIZZLES);
    }
    else if (you.inv[you.equip[EQ_WEAPON]].sub_type != CORPSE_SKELETON)
        mpr("The corpse collapses into a mass of pulpy flesh.");
    else
    {
        // practical max of 100 * 15 + 3000 = 4500
        // actual max of    200 * 15 + 3000 = 6000
        power *= 15;
        power += mons_weight( you.inv[you.equip[EQ_WEAPON]].plus );

        mpr("The skeleton explodes into sharp fragments of bone!");

        dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );
        zapping(ZAP_BONE_SHARDS, power, beam);

        success = true;
    }

    return (success);
}                               // end cast_bone_shards()

void sublimation(int power)
{
    unsigned char loopy = 0;    // general purpose loop variable {dlb}

    if (you.equip[EQ_WEAPON] == -1
        || you.inv[you.equip[EQ_WEAPON]].base_type != OBJ_FOOD
        || you.inv[you.equip[EQ_WEAPON]].sub_type != FOOD_CHUNK)
    {
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr( "A conflicting enchantment prevents the spell from "
                 "coming into effect." );
        }
        else if (!enough_hp( 2, true ))
        {
             mpr("Your attempt to draw power from your own body fails.");
        }
        else
        {
            mpr("You draw magical energy from your own body!");

            while (you.magic_points < you.max_magic_points && you.hp > 1)
            {
                inc_mp(1, false);
                dec_hp(1, false);

                for (loopy = 0; loopy < (you.hp > 1 ? 3 : 0); loopy++)
                {
                    if (random2(power) < 6)
                        dec_hp(1, false);
                }

                if (random2(power) < 6)
                    break;
            }
        }
    }
    else
    {
        mpr("The chunk of flesh you are holding crumbles to dust.");
        mpr("A flood of magical energy pours into your mind!");

        inc_mp( 7 + random2(7), false );

        dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );
    }

    return;
}                               // end sublimation()

// Simulacrum
//
// This spell extends creating undead to Ice mages, as such it's high
// level, requires wielding of the material component, and the undead 
// aren't overly powerful (they're also vulnerable to fire).  I've put
// back the abjuration level in order to keep down the army sizes again.
//
// As for what it offers necromancers considering all the downsides
// above... it allows the turning of a single corpse into an army of
// monsters (one per food chunk)... which is also a good reason for
// why it's high level.
//
// Hides and other "animal part" items are intentionally left out, it's
// unrequired complexity, and fresh flesh makes more "sense" for a spell
// reforming the original monster out of ice anyways.
void simulacrum(int power)
{
    int max_num = 4 + random2(power) / 20;
    if (max_num > 8)
        max_num = 8;

    const int chunk = you.equip[EQ_WEAPON];

    if (chunk != -1
        && is_valid_item( you.inv[ chunk ] )
        && (you.inv[ chunk ].base_type == OBJ_CORPSES
            || (you.inv[ chunk ].base_type == OBJ_FOOD
                && you.inv[ chunk ].sub_type == FOOD_CHUNK)))
    {
        const int mons_type = you.inv[ chunk ].plus;

        // Can't create more than the available chunks
        if (you.inv[ chunk ].quantity < max_num)
            max_num = you.inv[ chunk ].quantity;

        dec_inv_item_quantity( chunk, max_num );

        int summoned = 0;

        for (int i = 0; i < max_num; i++)
        {
            if (create_monster( MONS_SIMULACRUM_SMALL, 6,
                                BEH_FRIENDLY, you.x_pos, you.y_pos,
                                you.pet_target, mons_type ) != -1)
            {
                summoned++;
            }
        }

        if (summoned)
        {
            mprf("%s before you!",
                 (summoned == 1) ? "An icy figure forms "
                                 : "Some icy figures form ");
        }
        else
            mpr( "You feel cold for a second." );
    }
    else
    {
        mpr( "You need to wield a piece of raw flesh for this spell "
             "to be effective!" );
    }
}

void dancing_weapon(int pow, bool force_hostile)
{
    int numsc = std::min(2 + (random2(pow) / 5), 6);

    int summs = 0;
    beh_type behavi = BEH_FRIENDLY;
    bool failed = false;

    const int wpn = you.equip[EQ_WEAPON];

    // See if wielded item is appropriate:
    if (wpn == -1
        || you.inv[wpn].base_type != OBJ_WEAPONS
        || is_range_weapon( you.inv[wpn] )
        || is_fixed_artefact( you.inv[wpn] ))
    {
        failed = true;
    }

    // See if we can get an mitm for the dancing weapon:
    int i = get_item_slot();
    if (i == NON_ITEM)
        failed = true;

    if ( !failed )
    {

        // cursed weapons become hostile
        if (item_cursed( you.inv[wpn] ) || force_hostile)
            behavi = BEH_HOSTILE; 
        
        summs = create_monster( MONS_DANCING_WEAPON, numsc, behavi, 
                                you.x_pos, you.y_pos, you.pet_target, 1 );
        if ( summs == -1 )
            failed = true;            
    }

    if ( failed )
    {
        destroy_item(i);
        if ( wpn != -1 )
            mpr("Your weapon vibrates crazily for a second.");
        else
            msg::stream << "Your " << your_hand(true) << " twitch."
                        << std::endl;
        return;
    }

    // We are successful:
    unwield_item(); // unwield the weapon (including removing wield effects)

    // copy item (done here after any wield effects are removed)
    mitm[i] = you.inv[wpn];
    mitm[i].quantity = 1;
    mitm[i].x = 0;
    mitm[i].y = 0;
    mitm[i].link = NON_ITEM;

    // Mark the weapon as thrown so we'll autograb it when the tango's done.
    mitm[i].flags |= ISFLAG_THROWN;

    mprf("%s dances into the air!", you.inv[wpn].name(DESC_CAP_YOUR).c_str());

    you.inv[ wpn ].quantity = 0;

    menv[summs].inv[MSLOT_WEAPON] = i;
    menv[summs].colour = mitm[i].colour;
    burden_change();
}                               // end dancing_weapon()

static bool monster_on_level(int monster)
{
    for (int i = 0; i < MAX_MONSTERS; i++)
    {
        if (menv[i].type == monster)
            return true;
    }

    return false;
}                               // end monster_on_level()

// XXX: Relies on RUNE_xxx == BRANCH_xxx. See rune_type in enum.h.
static bool player_has_rune(branch_type branch)
{
    for (int i = 0; i < ENDOFPACK; i++)
        if (is_valid_item( you.inv[i] )
            && you.inv[i].base_type == OBJ_MISCELLANY
            && you.inv[i].sub_type == MISC_RUNE_OF_ZOT
            && you.inv[i].plus == branch)
        {
            return (true);
        }
    return (false);
}

//
// This function returns true if the player can use controlled
// teleport here.
//
bool allow_control_teleport( bool silent )
{
    bool ret = true;

    if (you.level_type == LEVEL_ABYSS || you.level_type == LEVEL_LABYRINTH)
        ret = false;
    else
    {
        switch (you.where_are_you)
        {
        case BRANCH_TOMB:
            ret = player_has_rune(you.where_are_you);
            break;

        case BRANCH_COCYTUS:
        case BRANCH_DIS:
        case BRANCH_TARTARUS:
        case BRANCH_GEHENNA:
            if (player_branch_depth() == branches[you.where_are_you].depth)
                ret = player_has_rune(you.where_are_you);
            break;
            
        case BRANCH_SLIME_PITS:
            // Cannot teleport into the slime pit vaults until
            // royal jelly is gone.
            if (monster_on_level(MONS_ROYAL_JELLY))
                ret = false;
            break;

        case BRANCH_ELVEN_HALLS:
            // Cannot raid the elven halls vaults until fountain drained
            if (player_branch_depth() == branches[BRANCH_ELVEN_HALLS].depth)
            {
                for (int x = 5; x < GXM - 5; x++) 
                {
                    for (int y = 5; y < GYM - 5; y++) 
                    {
                        if (grd[x][y] == DNGN_SPARKLING_FOUNTAIN)
                            ret = false;
                    }
                }
            }
            break;

        case BRANCH_HALL_OF_ZOT:
            // Cannot control teleport until the Orb is picked up
            if (player_branch_depth() == branches[BRANCH_HALL_OF_ZOT].depth
                && you.char_direction != GDT_ASCENDING)
            {
                ret = false;
            }
            break;

        default:
            break;
        }
    }

    // Tell the player why if they have teleport control.
    if (!ret && player_control_teleport() && !silent)
        mpr("A powerful magic prevents control of your teleportation.");

    return ret;
}                               // end allow_control_teleport()

void you_teleport(void)
{
    if (scan_randarts(RAP_PREVENT_TELEPORTATION))
        mpr("You feel a weird sense of stasis.");
    else if (you.duration[DUR_TELEPORT])
    {
        mpr("You feel strangely stable.");
        you.duration[DUR_TELEPORT] = 0;
    }
    else
    {
        mpr("You feel strangely unstable.");

        you.duration[DUR_TELEPORT] = 3 + random2(3);

        if (you.level_type == LEVEL_ABYSS && !one_chance_in(5))
        {
            mpr("You have a feeling this translocation may take a while to kick in...");
            you.duration[DUR_TELEPORT] += 5 + random2(10);
        }
    }

    return;
}                               // end you_teleport()

static bool teleport_player( bool allow_control, bool new_abyss_area )
{
    bool is_controlled = (allow_control && !you.duration[DUR_CONF]
                          && player_control_teleport()
                          && allow_control_teleport());

    if (scan_randarts(RAP_PREVENT_TELEPORTATION))
    {
        mpr("You feel a strange sense of stasis.");
        return false;
    }

    // after this point, we're guaranteed to teleport. Kill the appropriate
    // delays.
    interrupt_activity( AI_TELEPORT );

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
    {
        you.duration[DUR_CONDENSATION_SHIELD] = 0;
        you.redraw_armour_class = 1;
    }

    if (you.level_type == LEVEL_ABYSS)
    {
        abyss_teleport( new_abyss_area );
        you.pet_target = MHITNOT;
        return true;
    }

    FixedVector < int, 2 > plox;

    plox[0] = 1;
    plox[1] = 0;

    if (is_controlled)
    {
        mpr("You may choose your destination (press '.' or delete to select).");
        mpr("Expect minor deviation.");
        more();

        show_map(plox, false);

        redraw_screen();

#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Target square (%d,%d)", plox[0], plox[1] );
#endif

        plox[0] += random2(3) - 1;
        plox[1] += random2(3) - 1;

        if (one_chance_in(4))
        {
            plox[0] += random2(3) - 1;
            plox[1] += random2(3) - 1;
        }

        if (plox[0] < 6 || plox[1] < 6 || plox[0] > (GXM - 5)
                || plox[1] > (GYM - 5))
        {
            mpr("Nearby solid objects disrupt your rematerialisation!");
            is_controlled = false;
        }

#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "Scattered target square (%d,%d)", plox[0], plox[1] );
#endif

        if (is_controlled)
        {
            // no longer held in net
            if (plox[0] != you.x_pos || plox[1] != you.y_pos)
                clear_trapping_net();

            you.moveto(plox[0], plox[1]);

            if ((grd[you.x_pos][you.y_pos] != DNGN_FLOOR
                    && grd[you.x_pos][you.y_pos] != DNGN_SHALLOW_WATER)
                || mgrd[you.x_pos][you.y_pos] != NON_MONSTER
                || env.cgrid[you.x_pos][you.y_pos] != EMPTY_CLOUD)
            {
                is_controlled = false;
            }
            else
            {
                // controlling teleport contaminates the player -- bwr
                contaminate_player(1);
            }
        }
    }                           // end "if is_controlled"

    if (!is_controlled)
    {
        int newx, newy;

        do
        {
            newx = 5 + random2( GXM - 10 );
            newy = 5 + random2( GYM - 10 );
        }
        while ((grd[newx][newy] != DNGN_FLOOR
                && grd[newx][newy] != DNGN_SHALLOW_WATER)
               || mgrd[newx][newy] != NON_MONSTER
               || env.cgrid[newx][newy] != EMPTY_CLOUD);
               
        // no longer held in net
        if (newx != you.x_pos || newy != you.y_pos)
            clear_trapping_net();

        if ( newx == you.x_pos && newy == you.y_pos )
            mpr("Your surroundings flicker for a moment.");
        else if ( see_grid(newx, newy) )
            mpr("Your surroundings seem slightly different.");
        else
            mpr("Your surroundings suddenly seem different.");

        you.x_pos = newx;
        you.y_pos = newy;

        // Necessary to update the view centre.
        you.moveto(you.pos());
    }

    return !is_controlled;
}

void you_teleport_now( bool allow_control, bool new_abyss_area )
{
    const bool randtele = teleport_player(allow_control, new_abyss_area);
    // Xom is amused by uncontrolled teleports that land you in a
    // dangerous place.
    if (randtele && player_in_a_dangerous_place())
        xom_is_stimulated(255);
}

bool entomb(int powc)
{
    int number_built = 0;

    const dungeon_feature_type safe_to_overwrite[] = {
        DNGN_FLOOR, DNGN_SHALLOW_WATER, DNGN_OPEN_DOOR,
        DNGN_TRAP_MECHANICAL, DNGN_TRAP_MAGICAL, DNGN_TRAP_III,
        DNGN_UNDISCOVERED_TRAP,
        DNGN_FLOOR_SPECIAL
    };

    if ( powc > 95 )
        powc = 95;
    if ( powc < 25 )
        powc = 25;

    for (int srx = you.x_pos - 1; srx < you.x_pos + 2; srx++)
    {
        for (int sry = you.y_pos - 1; sry < you.y_pos + 2; sry++)
        {
            // tile already occupied by monster or yourself {dlb}:
            if (mgrd[srx][sry] != NON_MONSTER
                    || (srx == you.x_pos && sry == you.y_pos))
            {
                continue;
            }

            if ( random2(100) > powc )
                continue;

            bool proceed = false;
            for (unsigned int i = 0; i < ARRAYSIZE(safe_to_overwrite); ++i)
            {
                if (grd[srx][sry] == safe_to_overwrite[i])
                {
                    proceed = true;
                    break;
                }
            }

            // checkpoint one - do we have a legitimate tile? {dlb}
            if (!proceed)
                continue;

            int objl = igrd[srx][sry];
            int hrg = 0;

            while (objl != NON_ITEM)
            {
                // hate to see the orb get  detroyed by accident {dlb}:
                if (mitm[objl].base_type == OBJ_ORBS)
                {
                    proceed = false;
                    break;
                }

                hrg = mitm[objl].link;
                objl = hrg;
            }

            // checkpoint two - is the orb resting in the tile? {dlb}:
            if (!proceed)
                continue;

            objl = igrd[srx][sry];
            hrg = 0;

            while (objl != NON_ITEM)
            {
                hrg = mitm[objl].link;
                destroy_item(objl);
                objl = hrg;
            }

            // deal with clouds {dlb}:
            if (env.cgrid[srx][sry] != EMPTY_CLOUD)
                delete_cloud( env.cgrid[srx][sry] );

            // mechanical traps are destroyed {dlb}:
            int which_trap;
            if ((which_trap = trap_at_xy(srx, sry)) != -1)
            {
                if (trap_category(env.trap[which_trap].type)
                                                    == DNGN_TRAP_MECHANICAL)
                {
                    env.trap[which_trap].type = TRAP_UNASSIGNED;
                    env.trap[which_trap].x = 1;
                    env.trap[which_trap].y = 1;
                }
            }

            // finally, place the wall {dlb}:
            grd[srx][sry] = DNGN_ROCK_WALL;
            number_built++;
        }                       // end "for srx,sry"
    }

    if (number_built > 0)
        mpr("Walls emerge from the floor!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (number_built > 0);
}                               // end entomb()

void cast_poison_ammo(void)
{
    const int ammo = you.equip[EQ_WEAPON];

    if (ammo == -1
        || you.inv[ammo].base_type != OBJ_MISSILES
        || get_ammo_brand( you.inv[ammo] ) != SPMSL_NORMAL
        || you.inv[ammo].sub_type == MI_STONE
        || you.inv[ammo].sub_type == MI_SLING_BULLET
        || you.inv[ammo].sub_type == MI_LARGE_ROCK
        || you.inv[ammo].sub_type == MI_THROWING_NET)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    const char *old_desc = you.inv[ammo].name(DESC_CAP_YOUR).c_str();
    if (set_item_ego_type( you.inv[ammo], OBJ_MISSILES, SPMSL_POISONED ))
    {
        mprf("%s %s covered in a thin film of poison.", old_desc,
             (you.inv[ammo].quantity == 1) ? "is" : "are");
        you.wield_change = true;
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }
}                               // end cast_poison_ammo()

bool project_noise(void)
{
    bool success = false;
    FixedVector < int, 2 > plox;

    plox[0] = 1;
    plox[1] = 0;

    mpr( "Choose the noise's source (press '.' or delete to select)." );
    more();
    show_map(plox, false);

    redraw_screen();

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Target square (%d,%d)", plox[0], plox[1] );
#endif

    if (!silenced( plox[0], plox[1] ))
    {
        // player can use this spell to "sound out" the dungeon -- bwr
        if (plox[0] > 1 && plox[0] < (GXM - 2) 
            && plox[1] > 1 && plox[1] < (GYM - 2)
            && !grid_is_solid(grd[ plox[0] ][ plox[1] ]))
        {
            noisy( 30, plox[0], plox[1] );
            success = true;
        }

        if (!silenced( you.x_pos, you.y_pos ))
        {
            if (success)
                mprf(MSGCH_SOUND, "You hear a %svoice call your name.",
                     (see_grid( plox[0], plox[1] ) ? "distant " : "") );
            else
                mprf(MSGCH_SOUND, "You hear a dull thud.");
        }
    }

    return (success);
}                               // end project_noise()

/*
   Type recalled:
   0 = anything
   1 = undead only (Kiku religion ability)
   2 = orcs only (Beogh religion ability)
 */
bool recall(char type_recalled)
{
    int loopy = 0;              // general purpose looping variable {dlb}
    bool success = false;       // more accurately: "apparent success" {dlb}
    int start_count = 0;
    int step_value = 1;
    int end_count = (MAX_MONSTERS - 1);
    FixedVector < char, 2 > empty;
    struct monsters *monster = 0;       // NULL {dlb}

    empty[0] = empty[1] = 0;

// someone really had to make life difficult {dlb}:
// sometimes goes through monster list backwards
    if (coinflip())
    {
        start_count = (MAX_MONSTERS - 1);
        end_count = 0;
        step_value = -1;
    }

    for (loopy = start_count; loopy != end_count; loopy += step_value)
    {
        monster = &menv[loopy];

        if (monster->type == -1)
            continue;

        if (!mons_friendly(monster))
            continue;

        if (!monster_habitable_grid(monster, DNGN_FLOOR))
            continue;

        if (type_recalled == 1) // undead
        {
            if (monster->type != MONS_REAPER
                && mons_holiness(monster) != MH_UNDEAD)
            {
                continue;
            }
        }
        else if (type_recalled == 2) // Beogh
        {
            if (mons_species(monster->type) != MONS_ORC)
                continue;
                
            // does not include charmed orcs
            if (monster->attitude != ATT_FRIENDLY)
                continue;
        }

        if (empty_surrounds(you.x_pos, you.y_pos, DNGN_FLOOR, 3, false, empty))
        {
            // clear old cell pointer -- why isn't there a function for moving a monster?
            mgrd[monster->x][monster->y] = NON_MONSTER;
            // set monster x,y to new value
            monster->x = empty[0];
            monster->y = empty[1];
            // set new monster grid pointer to this monster.
            mgrd[monster->x][monster->y] = monster_index(monster);

            // only informed if monsters recalled are visible {dlb}:
            if (simple_monster_message(monster, " is recalled."))
                success = true;
        }
        else
        {
            break;              // no more room to place monsters {dlb}
        }
    }

    if (!success)
        mpr("Nothing appears to have answered your call.");

    return (success);
}                               // end recall()

int portal(void)
{
    char dir_sign = 0;
    unsigned char keyi;
    int target_level = 0;
    int old_level = you.your_level;

    if (!player_in_branch( BRANCH_MAIN_DUNGEON ))
    {
        mpr("This spell doesn't work here.");
        return (-1);
    }
    else if (grd[you.x_pos][you.y_pos] != DNGN_FLOOR)
    {
        mpr("You must find a clear area in which to cast this spell.");
        return (-1);
    }
    else
    {
        // the first query {dlb}:
        mpr("Which direction ('<' for up, '>' for down, 'x' to quit)?", MSGCH_PROMPT);

        for (;;)
        {
            keyi = get_ch();

            if (keyi == '<')
            {
                if (you.your_level == 0)
                    mpr("You can't go any further upwards with this spell.");
                else
                {
                    dir_sign = -1;
                    break;
                }
            }

            if (keyi == '>')
            {
                if (you.your_level == 35)
                    mpr("You can't go any further downwards with this spell.");
                else
                {
                    dir_sign = 1;
                    break;
                }
            }

            if (keyi == 'x')
            {
                canned_msg(MSG_OK);
                return (-1);         // an early return {dlb}
            }
        }

        // the second query {dlb}:
        mpr("How many levels (1 - 9, 'x' to quit)?", MSGCH_PROMPT);

        for (;;)
        {
            keyi = get_ch();

            if (keyi == 'x')
            {
                canned_msg(MSG_OK);
                return (-1);         // another early return {dlb}
            }

            if (!(keyi < '1' || keyi > '9'))
            {
                target_level = you.your_level + ((keyi - '0') * dir_sign);
                break;
            }
        }

        // actual handling begins here {dlb}:
        if (player_in_branch( BRANCH_MAIN_DUNGEON ))
        {
            if (target_level < 0)
                target_level = 0;
            else if (target_level > 26)
                target_level = 26;
        }

        mpr( "You fall through a mystic portal, and materialise at the "
             "foot of a staircase." );
        more();

        you.your_level = target_level - 1;

        down_stairs( old_level, DNGN_STONE_STAIRS_DOWN_I );
    }

    return (1);
}                               // end portal()

bool cast_death_channel(int power)
{
    bool success = false;

    if (you.duration[DUR_DEATH_CHANNEL] < 30)
    {
        mpr("Malign forces permeate your being, awaiting release.");

        you.duration[DUR_DEATH_CHANNEL] += 15 + random2(1 + (power / 3));

        if (you.duration[DUR_DEATH_CHANNEL] > 100)
            you.duration[DUR_DEATH_CHANNEL] = 100;

        success = true;
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }

    return (success);
}                               // end cast_death_channel()
