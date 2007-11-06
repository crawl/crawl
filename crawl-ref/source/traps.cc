/*
 *  File:       traps.cc
 *  Summary:    Traps related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     9/11/07        MPC             Split from misc.cc
 */

#include "AppHdr.h"

#include "externs.h"
#include "traps.h"

#include "beam.h"
#include "direct.h"
#include "it_use2.h"
#include "items.h"
#include "itemprop.h"
#include "makeitem.h"
#include "misc.h"
#include "mon-util.h"
#include "monstuff.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "skills.h"
#include "spells3.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "terrain.h"
#include "transfor.h"
#include "tutorial.h"
#include "view.h"

static void dart_trap(bool trap_known, int trapped, bolt &pbolt, bool poison);

// returns the number of a net on a given square
// if trapped only stationary ones are counted
// otherwise the first net found is returned
int get_trapping_net(int x, int y, bool trapped)
{
    int net, next;

    for (net = igrd[x][y]; net != NON_ITEM; net = next)
    {
         next = mitm[net].link;

         if (mitm[net].base_type == OBJ_MISSILES
             && mitm[net].sub_type == MI_THROWING_NET
             && (!trapped || item_is_stationary(mitm[net])))
         {
             return (net);
         }
    }
    return (NON_ITEM);
}

// if there are more than one net on this square
// split off one of them for checking/setting values
static void maybe_split_nets(item_def &item, int x, int y)
{
    if (item.quantity == 1)
    {
        set_item_stationary(item);
        return;
    }

    item_def it;

    it.base_type = item.base_type;
    it.sub_type = item.sub_type;
    it.plus = item.plus;
    it.plus2 = item.plus2;
    it.flags = item.flags;
    it.special = item.special;
    it.quantity = --item.quantity;
    item_colour(it);

    item.quantity = 1;
    set_item_stationary(item);

    copy_item_to_grid( it, x, y );
}

void mark_net_trapping(int x, int y)
{
    int net = get_trapping_net(x,y);
    if (net == NON_ITEM)
    {
        net = get_trapping_net(x,y, false);
        if (net != NON_ITEM)
            maybe_split_nets(mitm[net], x, y);
    }
}

void monster_caught_in_net(monsters *mon, bolt &pbolt)
{
    if (mon->body_size(PSIZE_BODY) >= SIZE_GIANT)
        return;

    if (mons_is_insubstantial(mon->type))
    {
        if (mons_near(mon) && player_monster_visible(mon))
            mprf("The net passes right through %s!", mon->name(DESC_NOCAP_THE).c_str());
        return;
    }

    const monsters* mons = static_cast<const monsters*>(mon);
    bool mon_flies = mons->flight_mode() == FL_FLY;
    if (mon_flies && (!mons_is_confused(mons) || one_chance_in(3)))
    {
        simple_monster_message(mon, " darts out from under the net!");
        return;
    }

    if (mons->type == MONS_OOZE || mons->type == MONS_PULSATING_LUMP)
    {
        simple_monster_message(mon, " oozes right through the net!");
        return;
    }

    if (!mons_is_caught(mon) && mon->add_ench(ENCH_HELD))
    {
        if (mons_near(mon) && !player_monster_visible(mon))
            mpr("Something gets caught in the net!");
        else
            simple_monster_message(mon, " is caught in the net!");

        if (mon_flies)
        {
            simple_monster_message(mon, " falls like a stone!");
            mons_check_pool(mon, pbolt.killer(), pbolt.beam_source);
        }
    }
}

void player_caught_in_net()
{
    if (you.body_size(PSIZE_BODY) >= SIZE_GIANT)
        return;

    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_AIR)
    {
        mpr("The net passes right through you!");
        return;
    }

    if (you.flight_mode() == FL_FLY && (!you.confused() || one_chance_in(3)))
    {
        mpr("You dart out from under the net!");
        return;
    }

    if (!you.attribute[ATTR_HELD])
    {
        you.attribute[ATTR_HELD] = 10;
        mpr("You become entangled in the net!");
        stop_running();

        // I guess levitation works differently, keeping both you
        // and the net hovering above the floor
        if (you.flight_mode() == FL_FLY)
        {
            mpr("You fall like a stone!");
            fall_into_a_pool(you.x_pos, you.y_pos, false, grd(you.pos()));
        }
    }
}

static void dart_trap(bool trap_known, int trapped, bolt &pbolt, bool poison)
{
    int damage_taken = 0;
    int trap_hit, your_dodge;

    if (one_chance_in(5) || (trap_known && !one_chance_in(4)))
    {
        mprf( "You avoid triggering a%s trap.", pbolt.name.c_str() );
        return;
    }

    if (you.equip[EQ_SHIELD] != -1 && one_chance_in(3))
        exercise( SK_SHIELDS, 1 );

    std::string msg = "A" + pbolt.name + " shoots out and ";

    if (random2( 20 + 5 * you.shield_blocks * you.shield_blocks ) 
                                                < player_shield_class())
    {
        you.shield_blocks++;
        msg += "hits your shield.";
        mpr(msg.c_str());
    }
    else
    {
        // note that this uses full ( not random2limit(foo,40) )
        // player_evasion.
        trap_hit = (20 + (you.your_level * 2)) * random2(200) / 100;

        your_dodge = player_evasion() + random2(you.dex) / 3
            - 2 + (you.duration[DUR_REPEL_MISSILES] * 10);
        
        if (trap_hit >= your_dodge && you.duration[DUR_DEFLECT_MISSILES] == 0)
        {
            msg += "hits you!";
            mpr(msg.c_str());
            
            if (poison && random2(100) < 50 - (3 * player_AC()) / 2
                && !player_res_poison())
            {
                poison_player( 1 + random2(3) );
            }
            
            damage_taken = roll_dice( pbolt.damage );
            damage_taken -= random2( player_AC() + 1 );
            
            if (damage_taken > 0)
                ouch( damage_taken, 0, KILLED_BY_TRAP, pbolt.name.c_str() );
        }
        else
        {
            msg += "misses you.";
            mpr(msg.c_str());
        }

        if (player_light_armour(true) && coinflip())
            exercise( SK_DODGING, 1 );
    }

    pbolt.target_x = you.x_pos;
    pbolt.target_y = you.y_pos;

    if (coinflip())
        itrap( pbolt, trapped );
}                               // end dart_trap()

//
// itrap takes location from target_x, target_y of bolt strcture.
//

void itrap( struct bolt &pbolt, int trapped )
{
    object_class_type base_type = OBJ_MISSILES;
    int sub_type = MI_DART;

    switch (env.trap[trapped].type)
    {
    case TRAP_DART:
        base_type = OBJ_MISSILES;
        sub_type = MI_DART;
        break;
    case TRAP_ARROW:
        base_type = OBJ_MISSILES;
        sub_type = MI_ARROW;
        break;
    case TRAP_BOLT:
        base_type = OBJ_MISSILES;
        sub_type = MI_BOLT;
        break;
    case TRAP_SPEAR:
        base_type = OBJ_WEAPONS;
        sub_type = WPN_SPEAR;
        break;
    case TRAP_AXE:
        base_type = OBJ_WEAPONS;
        sub_type = WPN_HAND_AXE;
        break;
    case TRAP_NEEDLE:
        base_type = OBJ_MISSILES;
        sub_type = MI_NEEDLE;
        break;
    case TRAP_NET:
        base_type = OBJ_MISSILES;
        sub_type = MI_THROWING_NET;
        break;
    default:
        return;
    }

    trap_item( base_type, sub_type, pbolt.target_x, pbolt.target_y );

    return;
}                               // end itrap()

void handle_traps(char trt, int i, bool trap_known)
{
    struct bolt beam;

    switch (trt)
    {
    case TRAP_DART:
        beam.name = " dart";
        beam.damage = dice_def( 1, 4 + (you.your_level / 2) );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_NEEDLE:
        beam.name = " needle";
        beam.damage = dice_def( 1, 0 );
        dart_trap(trap_known, i, beam, true);
        break;

    case TRAP_ARROW:
        beam.name = "n arrow";
        beam.damage = dice_def( 1, 7 + you.your_level );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_BOLT:
        beam.name = " bolt";
        beam.damage = dice_def( 1, 13 + you.your_level );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_SPEAR:
        beam.name = " spear";
        beam.damage = dice_def( 1, 10 + you.your_level );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_AXE:
        beam.name = "n axe";
        beam.damage = dice_def( 1, 15 + you.your_level );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_TELEPORT:
        mpr("You enter a teleport trap!");

        if (scan_randarts(RAP_PREVENT_TELEPORTATION))
            mpr("You feel a weird sense of stasis.");
        else
            you_teleport_now( true );
        break;

    case TRAP_AMNESIA:
        mpr("You feel momentarily disoriented.");
        if (!wearing_amulet(AMU_CLARITY))
            forget_map(random2avg(100, 2));
        break;

    case TRAP_BLADE:
        if (trap_known && one_chance_in(3))
            mpr("You avoid triggering a blade trap.");
        else if (random2limit(player_evasion(), 40)
                        + (random2(you.dex) / 3) + (trap_known ? 3 : 0) > 8)
        {
            mpr("A huge blade swings just past you!");
        }
        else
        {
            mpr("A huge blade swings out and slices into you!");
            ouch( (you.your_level * 2) + random2avg(29, 2)
                    - random2(1 + player_AC()), 0, KILLED_BY_TRAP, " blade" );
        }
        break;

    case TRAP_NET:

        if (trap_known && one_chance_in(3))
            mpr("A net swings high above you.");
        else
        {
            if (random2limit(player_evasion(), 40)
                        + (random2(you.dex) / 3) + (trap_known ? 3 : 0) > 12)
            {
                mpr("A net drops to the ground!");
            }
            else
            {
                mpr("A large net falls onto you!");
                player_caught_in_net();
            }

            trap_item( OBJ_MISSILES, MI_THROWING_NET, env.trap[i].x, env.trap[i].y );
            if (you.attribute[ATTR_HELD])
                mark_net_trapping(you.x_pos, you.y_pos);

            grd[env.trap[i].x][env.trap[i].y] = DNGN_FLOOR;
            env.trap[i].type = TRAP_UNASSIGNED;
        }
        break;
        
    case TRAP_ZOT:
    default:
        mpr((trap_known) ? "You enter the Zot trap."
                         : "Oh no! You have blundered into a Zot trap!");
        miscast_effect( SPTYP_RANDOM, random2(30) + you.your_level,
                        75 + random2(100), 3, "a Zot trap" );
        break;
    }
    learned_something_new(TUT_SEEN_TRAP, you.x_pos, you.y_pos);
}                               // end handle_traps()

void disarm_trap( struct dist &disa )
{
    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    int i, j;

    for (i = 0; i < MAX_TRAPS; i++)
    {
        if (env.trap[i].x == you.x_pos + disa.dx
            && env.trap[i].y == you.y_pos + disa.dy)
        {
            break;
        }

        if (i == MAX_TRAPS - 1)
        {
            mpr("Error - couldn't find that trap.");
            return;
        }
    }

    if (trap_category(env.trap[i].type) == DNGN_TRAP_MAGICAL)
    {
        mpr("You can't disarm that trap.");
        return;
    }

    if (random2(you.skills[SK_TRAPS_DOORS] + 2) <= random2(you.your_level + 5))
    {
        mpr("You failed to disarm the trap.");

        you.turn_is_over = true;

        if (random2(you.dex) > 5 + random2(5 + you.your_level))
            exercise(SK_TRAPS_DOORS, 1 + random2(you.your_level / 5));
        else
        {
            if (env.trap[i].type == TRAP_NET &&
                (env.trap[i].x != you.x_pos || env.trap[i].y != you.y_pos))
            {
                if (coinflip())
                    return;

                mpr("You stumble into the trap!");
                move_player_to_grid( env.trap[i].x, env.trap[i].y, true, false, true);
            }
            else
                handle_traps(env.trap[i].type, i, false);

            if (coinflip())
                exercise(SK_TRAPS_DOORS, 1);
        }

        return;
    }

    mpr("You have disarmed the trap.");

    struct bolt beam;

    beam.target_x = you.x_pos + disa.dx;
    beam.target_y = you.y_pos + disa.dy;

    if (env.trap[i].type == TRAP_NET)
        trap_item( OBJ_MISSILES, MI_THROWING_NET, beam.target_x, beam.target_y );
    else if (env.trap[i].type != TRAP_BLADE
        && trap_category(env.trap[i].type) == DNGN_TRAP_MECHANICAL)
    {
        const int num_to_make = 10 + random2(you.skills[SK_TRAPS_DOORS]);
        for (j = 0; j < num_to_make; j++)
        {           
            // places items (eg darts), which will automatically stack
            itrap(beam, i);
        }
    }

    grd[you.x_pos + disa.dx][you.y_pos + disa.dy] = DNGN_FLOOR;
    env.trap[i].type = TRAP_UNASSIGNED;
    you.turn_is_over = true;

    // reduced from 5 + random2(5)
    exercise(SK_TRAPS_DOORS, 1 + random2(5) + (you.your_level / 5));
}                               // end disarm_trap()

// attempts to take a net off a given monster
// Do not expect gratitude for this!
// ----------------------------------
void remove_net_from(monsters *mon)
{
    you.turn_is_over = true;
    
    int net = get_trapping_net(mon->x, mon->y);

    if (net == NON_ITEM)
    {
        mon->del_ench(ENCH_HELD, true);
        return;
    }

    // factor in whether monster is paralysed or invisible
    int paralys = 0;
    if (mons_is_paralysed(mon)) // makes this easier
        paralys = random2(5);
        
    int invis = 0;
    if (!player_monster_visible(mon)) // makes this harder
        invis = 3 + random2(5);

    bool net_destroyed = false;
    if ( random2(you.skills[SK_TRAPS_DOORS] + 2) + paralys
           <= random2( 2*mon->body_size(PSIZE_BODY) + 3 ) + invis)
    {
        if (one_chance_in(you.skills[SK_TRAPS_DOORS] + you.dex/2))
        {
            mitm[net].plus--;
            mpr("You tear at the net.");
            if (mitm[net].plus < -7)
            {
                mpr("Whoops! The net comes apart in your hands!");
                mon->del_ench(ENCH_HELD, true);
                destroy_item(net);
                net_destroyed = true;
            }
        }

        if (!net_destroyed)
        {
            if (player_monster_visible(mon))
            {
                mprf("You fail to remove the net from %s.",
                     mon->name(DESC_NOCAP_THE).c_str());
            }
            else
                mpr("You fail to remove the net.");
        }

        if (random2(you.dex) > 5 + random2( 2*mon->body_size(PSIZE_BODY) ))
            exercise(SK_TRAPS_DOORS, 1 + random2(mon->body_size(PSIZE_BODY)/2));
        return;
    }
     
    mon->del_ench(ENCH_HELD, true);
    remove_item_stationary(mitm[net]);
    
    if (player_monster_visible(mon))
        mprf("You free %s.", mon->name(DESC_NOCAP_THE).c_str());
    else
        mpr("You loosen the net.");

}

// decides whether you will try to tear the net (result <= 0)
// or try to slip out of it (result > 0)
// both damage and escape could be 9 (more likely for damage)
// but are capped at 5 (damage) and 4 (escape)
static int damage_or_escape_net(int hold)
{
    // Spriggan: little (+2)
    // Halfling, Kobold, Gnome: small (+1)
    // Human, Elf, ...: medium (0)
    // Ogre, Troll, Centaur, Naga: large (-1)
    // transformations: spider, bat: tiny (+3); ice beast: large (-1)
    int escape = SIZE_MEDIUM - you.body_size(PSIZE_BODY);
    
    int damage = -escape;

    if (escape == 0) // middle-sized creatures are at a disadvantage
    {
        escape += coinflip();
        damage += coinflip();
    }

    // your weapon may damage the net, max. bonus of 2
    if (you.equip[EQ_WEAPON] != -1)
    {
        if (can_cut_meat(you.inv[you.equip[EQ_WEAPON]]))
            damage++;
            
        int brand = get_weapon_brand( you.inv[you.equip[EQ_WEAPON]] );
        if (brand == SPWPN_FLAMING || brand == SPWPN_VORPAL)
            damage++;
    }
    else if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
        damage += 2;
    else if (you.has_usable_claws())
    {
        if (you.species == SP_TROLL || you.species == SP_GHOUL)
            damage += 2;
        else if (you.mutation[MUT_CLAWS] == 1)
            damage += coinflip();
        else
            damage += you.mutation[MUT_CLAWS] - 1;
    }
    
    // Berserkers get a fighting bonus
    if (you.duration[DUR_BERSERKER])
        damage += 2;

    // damaged nets are easier to slip out of
    if (hold < 0)
    {
        escape += random2(-hold/2) + 1;
        damage += random2(-hold/3) + 1; // ... and easier to destroy
    }
        
    // check stats
    if (you.strength > random2(18))
        damage++;
    if (you.dex > random2(12))
        escape++;
    if (player_evasion() > random2(20))
        escape++;

    // monsters around you add urgency
    if (!i_feel_safe())
    {
        damage++;
        escape++;
    }
    
    // confusion makes the whole thing somewhat harder
    // (less so for trying to escape)
    if (you.duration[DUR_CONF])
    {
        if (escape > 1)
            escape--;
        else if (damage >= 2)
            damage -= 2;
    }
    
    // if undecided, choose damaging approach (it's quicker)
    if (damage >= escape)
        return (-damage); // negate value
        
    return (escape);
}

// calls the above function to decide on how to get free
// note that usually the net will be damaged until trying to slip out
// becomes feasible (for size etc.), so it may take even longer
void free_self_from_net()
{
    int net = get_trapping_net(you.x_pos, you.y_pos);

    if (net == NON_ITEM) // really shouldn't happen!
    {
        you.attribute[ATTR_HELD] = 0;
        return;
    }

    int hold = mitm[net].plus;
    int do_what = damage_or_escape_net(hold);
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "net.plus: %d, ATTR_HELD: %d, do_what: %d",
         hold, you.attribute[ATTR_HELD], do_what);
#endif

    if (do_what <= 0) // you try to destroy the net
    {                 // for previously undamaged nets this takes at least 2 
                      // and at most 8 turns
        bool can_slice = you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS
                         || you.equip[EQ_WEAPON] != -1
                            && can_cut_meat(you.inv[you.equip[EQ_WEAPON]]);

        int damage = -do_what;
        if (damage < 1)
            damage = 1;
            
        if (you.duration[DUR_BERSERKER])
            damage *= 2;

        if (damage > 5)
            damage = 5;
                    
        hold -= damage;
        mitm[net].plus = hold;

        if (hold < -7)
        {
            mprf("You %s the net and break free!",
                 can_slice ? (damage >= 4? "slice" : "cut") :
                             (damage >= 4? "shred" : "rip"));
                  
            destroy_item(net);

            you.attribute[ATTR_HELD] = 0;
            return;
        }
        
        if (damage >= 4)
        {
            mprf("You %s into the net.",
                 can_slice? "slice" : "tear a large gash");
        }
        else
            mpr("You struggle against the net.");

        // occasionally decrease duration a bit
        // (this is so switching from damage to escape does not hurt as much)
        if (you.attribute[ATTR_HELD] > 1 && coinflip())
        {
            you.attribute[ATTR_HELD]--;
            
            if (you.attribute[ATTR_HELD] > 1 && hold < -random2(5))
                you.attribute[ATTR_HELD]--;
        }
   }
   else // you try to escape (takes at least 3 turns, and at most 10)
   {
        int escape = do_what;

        if (you.duration[DUR_HASTE]) // extra bonus, also Berserk
            escape++;
            
        if (escape < 1)
            escape = 1;
        else if (escape > 4)
            escape = 4;
            
        if (escape >= you.attribute[ATTR_HELD])
        {
            if (escape >= 3)
                mpr("You slip out of the net!");
            else
                mpr("You break free from the net!");
                
            you.attribute[ATTR_HELD] = 0;
            remove_item_stationary(mitm[net]);
            return;
        }
        
        if (escape >= 3)
            mpr("You try to slip out of the net.");
        else
            mpr("You struggle to escape the net.");

        you.attribute[ATTR_HELD] -= escape;
   }
}

void clear_trapping_net()
{
   if (!you.attribute[ATTR_HELD])
       return;
       
   const int net = get_trapping_net(you.x_pos, you.y_pos);
   if (net != NON_ITEM)
       remove_item_stationary(mitm[net]);

   you.attribute[ATTR_HELD] = 0;
}

bool trap_item(object_class_type base_type, char sub_type,
               char beam_x, char beam_y)
{
    item_def  item;

    item.base_type = base_type;
    item.sub_type = sub_type;
    item.plus = 0;
    item.plus2 = 0;
    item.flags = 0;
    item.special = 0;
    item.quantity = 1;

    if (base_type == OBJ_MISSILES)
    {
        if (sub_type == MI_NEEDLE)
            set_item_ego_type( item, OBJ_MISSILES, SPMSL_POISONED );
        else
            set_item_ego_type( item, OBJ_MISSILES, SPMSL_NORMAL );
    }
    else
    {
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_NORMAL );
    }

    item_colour(item);

    if (igrd[beam_x][beam_y] != NON_ITEM)
    {
        if (items_stack( item, mitm[ igrd[beam_x][beam_y] ] ))
        {
            inc_mitm_item_quantity( igrd[beam_x][beam_y], 1 );
            return (false);
        }

        // don't want to go overboard here. Will only generate up to three
        // separate trap items, or less if there are other items present.
        if (mitm[ igrd[beam_x][beam_y] ].link != NON_ITEM
            && (item.base_type != OBJ_MISSILES
                || item.sub_type != MI_THROWING_NET))
        {
            if (mitm[ mitm[ igrd[beam_x][beam_y] ].link ].link != NON_ITEM)
                return (false);
        }
    }                           // end of if igrd != NON_ITEM

    return (!copy_item_to_grid( item, beam_x, beam_y, 1 ));
}                               // end trap_item()

// returns appropriate trap symbol for a given trap type {dlb}
dungeon_feature_type trap_category(trap_type type)
{
    switch (type)
    {
    case TRAP_TELEPORT:
    case TRAP_AMNESIA:
    case TRAP_ZOT:
        return (DNGN_TRAP_MAGICAL);

    case TRAP_DART:
    case TRAP_ARROW:
    case TRAP_SPEAR:
    case TRAP_AXE:
    case TRAP_BLADE:
    case TRAP_BOLT:
    case TRAP_NEEDLE:
    case TRAP_NET:
    default:                    // what *would* be the default? {dlb}
        return (DNGN_TRAP_MECHANICAL);
    }
}                               // end trap_category()

// returns index of the trap for a given (x,y) coordinate pair {dlb}
int trap_at_xy(int which_x, int which_y)
{

    for (int which_trap = 0; which_trap < MAX_TRAPS; which_trap++)
    {
        if (env.trap[which_trap].x == which_x &&
            env.trap[which_trap].y == which_y &&
            env.trap[which_trap].type != TRAP_UNASSIGNED)
        {
            return (which_trap);
        }
    }

    // no idea how well this will be handled elsewhere: {dlb}
    return (-1);
}                               // end trap_at_xy()

trap_type trap_type_at_xy(int x, int y)
{
    const int idx = trap_at_xy(x, y);
    return (idx == -1? NUM_TRAPS : env.trap[idx].type);
}

