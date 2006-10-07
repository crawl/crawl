/*
 *  File:     delay.cc
 *  Summary:  Functions for handling multi-turn actions.    
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <1> Sept 09, 2001     BWR             Created
 */

#include "AppHdr.h"
#include "externs.h"

#include <stdio.h>
#include <string.h>

#include "delay.h"
#include "enum.h"
#include "food.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "it_use2.h"
#include "message.h"
#include "misc.h"
#include "monstuff.h"
#include "mstuff2.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "spl-util.h"
#include "stuff.h"

void start_delay( int type, int turns, int parm1, int parm2 )
/***********************************************************/
{
    delay_queue_item  delay;
    
    delay.type = type;
    delay.duration = turns;
    delay.parm1 = parm1;
    delay.parm2 = parm2;

    you.delay_queue.push( delay ); 
}

void stop_delay( void )
/*********************/
{
    delay_queue_item  delay = you.delay_queue.front(); 

    // At the very least we can remove any queued delays, right 
    // now there is no problem with doing this... note that
    // any queuing here can only happen from a single command,
    // as the effect of a delay doesn't normally allow interaction
    // until it is done... it merely chains up individual actions
    // into a single action.  -- bwr
    if (you.delay_queue.size() > 1) 
    {
        while (you.delay_queue.size())
            you.delay_queue.pop();

        you.delay_queue.push( delay );
    }

    switch (delay.type)
    {
    case DELAY_BUTCHER:
        // Corpse keeps track of work in plus2 field, see handle_delay() -- bwr
        mpr( "You stop butchering the corpse." );
        you.delay_queue.pop();
        break;

    case DELAY_MEMORISE:
        // Losing work here is okay... having to start from 
        // scratch is a reasonable behaviour. -- bwr
        mpr( "Your memorization is interrupted." );
        you.delay_queue.pop();
        break;

    case DELAY_PASSWALL:        
        // The lost work here is okay since this spell requires 
        // the player to "attune to the rock".  If changed, the
        // the delay should be increased to reduce the power of
        // this spell. -- bwr
        mpr( "Your meditation is interrupted." );
        you.delay_queue.pop();
        break;

    case DELAY_INTERUPTABLE:  
        // always stopable by definition... 
        // try using a more specific type anyways. -- bwr
        you.delay_queue.pop();
        break;

    case DELAY_EAT:
        // XXX: Large problems with object destruction here... food can
        // be from in the inventory or on the ground and these are
        // still handled quite differently.  Eventually we would like 
        // this to be stoppable, with partial food items implimented. -- bwr
        break; 

    case DELAY_ARMOUR_ON:
    case DELAY_ARMOUR_OFF:
        // These two have the default action of not being interuptable,
        // although they will often be chained (remove cloak, remove 
        // armour, wear new armour, replace cloak), all of which can
        // be stopped when complete.  This is a fairly reasonable 
        // behaviour, although perhaps the character should have 
        // option of reversing the current action if it would take 
        // less time to get out of the plate mail that's half on
        // than it would take to continue.  Probably too much trouble,
        // and would have to have a prompt... this works just fine. -- bwr
        break;

    case DELAY_AUTOPICKUP:        // one turn... too much trouble
    case DELAY_WEAPON_SWAP:       // one turn... too much trouble 
    case DELAY_DROP_ITEM:         // one turn... only used for easy armour drops
    case DELAY_ASCENDING_STAIRS:  // short... and probably what people want
    case DELAY_DESCENDING_STAIRS: // short... and probably what people want
    case DELAY_UNINTERUPTABLE:    // never stoppable 
    default:
        break;
    }
}

bool you_are_delayed( void )
/**************************/
{
    return (!you.delay_queue.empty());
}

int current_delay_action( void )
/******************************/
{
    return (you_are_delayed() ? you.delay_queue.front().type 
                              : DELAY_NOT_DELAYED);
}

void handle_delay( void )
/***********************/
{
    int   ego;
    char  str_pass[ ITEMNAME_SIZE ];

    if (you_are_delayed()) 
    {
        delay_queue_item &delay = you.delay_queue.front();

        // First check cases where delay may no longer be valid:
        // XXX: need to handle passwall when monster digs -- bwr
        if (delay.type == DELAY_BUTCHER)
        {
            // A monster may have raised the corpse you're chopping up! -- bwr
            // Note that a monster could have raised the corpse and another
            // monster could die and create a corpse with the same ID number...
            // However, it would not be at the player's square like the 
            // original and that's why we do it this way.  Note that 
            // we ignore the conversion to skeleton possibility just to 
            // be nice. -- bwr
            if (is_valid_item( mitm[ delay.parm1 ] )
                && mitm[ delay.parm1 ].base_type == OBJ_CORPSES
                && mitm[ delay.parm1 ].x == you.x_pos
                && mitm[ delay.parm1 ].y == you.y_pos )
            {
		// special < 100 is the rottenness check
		if ( (mitm[delay.parm1].special < 100) &&
		     (delay.parm2 >= 100) ) {
		    mpr("The corpse rots.", MSGCH_ROTTEN_MEAT);
		    delay.parm2 = 99; // don't give the message twice
		}

                // mark work done on the corpse in case we stop -- bwr
                mitm[ delay.parm1 ].plus2++;
            }
            else
            {
                // corpse is no longer valid!
                stop_delay();
                return;
            }
        }

        // Handle delay:
        if (delay.duration > 0)
        {
#if DEBUG_DIAGNOSTICS
            snprintf( info, INFO_SIZE, "Delay type: %d   duration: %d", 
                      delay.type, delay.duration ); 

            mpr( info, MSGCH_DIAGNOSTICS );
#endif
            switch ( delay.type )
            {
            case DELAY_ARMOUR_ON:
                in_name( delay.parm1, DESC_NOCAP_YOUR, str_pass );
                snprintf( info, INFO_SIZE,
                          "You continue putting on %s.", str_pass );
                mpr(info, MSGCH_MULTITURN_ACTION);
                break;
            case DELAY_ARMOUR_OFF:
                in_name( delay.parm1, DESC_NOCAP_YOUR, str_pass );
                snprintf( info, INFO_SIZE,
                          "You continue taking off %s.", str_pass );
                mpr(info, MSGCH_MULTITURN_ACTION);
                break;
            case DELAY_BUTCHER:
                mpr("You continue butchering the corpse.",
                    MSGCH_MULTITURN_ACTION);
                break;
            case DELAY_MEMORISE:
                mpr("You continue memorising.", MSGCH_MULTITURN_ACTION);
                break;
            case DELAY_PASSWALL:
                mpr("You continue meditating on the rock.",
                    MSGCH_MULTITURN_ACTION);
                break;
            default:
                break;
            }
            delay.duration--;
        }
        else 
        {
            switch (delay.type)
            {
            case DELAY_AUTOPICKUP:
                break;

            case DELAY_WEAPON_SWAP:
                weapon_switch( delay.parm1 );
                break;

            case DELAY_ARMOUR_ON:
            {
                set_ident_flags( you.inv[ delay.parm1 ], 
                                 ISFLAG_EQ_ARMOUR_MASK );

                in_name( delay.parm1, DESC_NOCAP_YOUR, str_pass ); 
                snprintf( info, INFO_SIZE, 
                        "You finish putting on %s.", str_pass );
                mpr(info);

                const equipment_type slot =
                        get_armour_slot( you.inv[delay.parm1] );

                if (slot == EQ_BODY_ARMOUR)
                {
                    you.equip[EQ_BODY_ARMOUR] = delay.parm1;

                    if (you.duration[DUR_ICY_ARMOUR] != 0)
                    {
                        mpr( "Your icy armour melts away.", MSGCH_DURATION );
                        you.redraw_armour_class = 1;
                        you.duration[DUR_ICY_ARMOUR] = 0;
                    }
                }
                else
                {
                    switch (slot)
                    {
                    case EQ_SHIELD:
                        if (you.duration[DUR_CONDENSATION_SHIELD])
                        {
                            mpr( "Your icy shield evaporates.", MSGCH_DURATION );
                            you.duration[DUR_CONDENSATION_SHIELD] = 0;
                        }
                        you.equip[EQ_SHIELD] = delay.parm1;
                        break;
                    case EQ_CLOAK:
                        you.equip[EQ_CLOAK] = delay.parm1;
                        break;
                    case EQ_HELMET:
                        you.equip[EQ_HELMET] = delay.parm1;
                        break;
                    case EQ_GLOVES:
                        you.equip[EQ_GLOVES] = delay.parm1;
                        break;
                    case EQ_BOOTS:
                        you.equip[EQ_BOOTS] = delay.parm1;
                        break;
                    default:
                        break;
                    }
                }

                ego = get_armour_ego_type( you.inv[ delay.parm1 ] );
                if (ego != SPARM_NORMAL)
                {   
                    switch (ego)
                    {
                    case SPARM_RUNNING:
                        strcpy(info, "You feel quick");
                        strcat(info, (you.species == SP_NAGA
                                || you.species == SP_CENTAUR) ? "." : " on your feet.");
                        mpr(info);
                        break;

                    case SPARM_FIRE_RESISTANCE:
                        mpr("You feel resistant to fire.");
                        break;

                    case SPARM_COLD_RESISTANCE:
                        mpr("You feel resistant to cold.");
                        break;

                    case SPARM_POISON_RESISTANCE:
                        mpr("You feel healthy.");
                        break;

                    case SPARM_SEE_INVISIBLE:
                        mpr("You feel perceptive.");
                        break;

                    case SPARM_DARKNESS:
                        if (!you.invis)
                            mpr("You become transparent for a moment.");
                        break;

                    case SPARM_STRENGTH:
                        modify_stat(STAT_STRENGTH, 3, false);
                        break;

                    case SPARM_DEXTERITY:
                        modify_stat(STAT_DEXTERITY, 3, false);
                        break;

                    case SPARM_INTELLIGENCE:
                        modify_stat(STAT_INTELLIGENCE, 3, false);
                        break;

                    case SPARM_PONDEROUSNESS:
                        mpr("You feel rather ponderous.");
                        // you.speed += 2; 
                        you.redraw_evasion = 1;
                        break;

                    case SPARM_LEVITATION:
                        mpr("You feel rather light.");
                        break;

                    case SPARM_MAGIC_RESISTANCE:
                        mpr("You feel resistant to magic.");
                        break;

                    case SPARM_PROTECTION:
                        mpr("You feel protected.");
                        break;

                    case SPARM_STEALTH:
                        mpr("You feel stealthy.");
                        break;

                    case SPARM_RESISTANCE:
                        mpr("You feel resistant to extremes of temperature.");
                        break;

                    case SPARM_POSITIVE_ENERGY:
                        mpr("Your life-force is being protected.");
                        break;

                    case SPARM_ARCHMAGI:
                        if (!you.skills[SK_SPELLCASTING])
                            mpr("You feel strangely numb.");
                        else
                            mpr("You feel extremely powerful.");
                        break;
                    }
                }

                if (is_random_artefact( you.inv[ delay.parm1 ] ))
                    use_randart( delay.parm1 );

                if (item_cursed( you.inv[ delay.parm1 ] ))
                    mpr( "Oops, that feels deathly cold." );

                you.redraw_armour_class = 1;
                you.redraw_evasion = 1;
                break;
            }
            case DELAY_ARMOUR_OFF:
            {
                in_name( delay.parm1, DESC_NOCAP_YOUR, str_pass ); 
                snprintf( info, INFO_SIZE, "You finish taking off %s.", str_pass );
                mpr(info);

                const equipment_type slot = 
                        get_armour_slot( you.inv[delay.parm1] );

                if (slot == EQ_BODY_ARMOUR)
                {
                    you.equip[EQ_BODY_ARMOUR] = -1;
                }
                else
                {
                    switch (slot)
                    {
                    case EQ_SHIELD:
                        if (delay.parm1 == you.equip[EQ_SHIELD])
                            you.equip[EQ_SHIELD] = -1;
                        break;

                    case EQ_CLOAK:
                        if (delay.parm1 == you.equip[EQ_CLOAK])
                            you.equip[EQ_CLOAK] = -1;
                        break;

                    case EQ_HELMET:
                        if (delay.parm1 == you.equip[EQ_HELMET])
                            you.equip[EQ_HELMET] = -1;
                        break;


                    case EQ_GLOVES:
                        if (delay.parm1 == you.equip[EQ_GLOVES])
                            you.equip[EQ_GLOVES] = -1;
                        break;

                    case EQ_BOOTS:
                        if (delay.parm1 == you.equip[EQ_BOOTS])
                            you.equip[EQ_BOOTS] = -1;
                        break;

                    default:
                        break;
                    }
                }

                unwear_armour( delay.parm1 );

                you.redraw_armour_class = 1;
                you.redraw_evasion = 1;
                break;
            }
            case DELAY_EAT:
                mpr( "You finish eating." );
                break; 

            case DELAY_MEMORISE:
                mpr( "You finish memorising." );
                add_spell_to_memory( delay.parm1 );
                break; 

            case DELAY_PASSWALL:
                {
                    mpr( "You finish merging with the rock." );
                    more();  // or the above message won't be seen

                    const int pass_x = delay.parm1;
                    const int pass_y = delay.parm2;

                    if (pass_x != 0 && pass_y != 0)
                    {

                        switch (grd[ pass_x ][ pass_y ])
                        {
                        case DNGN_ROCK_WALL:
                        case DNGN_STONE_WALL:
                        case DNGN_METAL_WALL:
                        case DNGN_GREEN_CRYSTAL_WALL:
                        case DNGN_WAX_WALL:
                        case DNGN_SILVER_STATUE:
                        case DNGN_ORANGE_CRYSTAL_STATUE:
                            ouch(1 + you.hp, 0, KILLED_BY_PETRIFICATION);
                            break;

                        case DNGN_SECRET_DOOR:      // oughtn't happen
                        case DNGN_CLOSED_DOOR:      // open the door
                            grd[ pass_x ][ pass_y ] = DNGN_OPEN_DOOR;
                            break;

                        default:
                            break;
                        }

                        // move any monsters out of the way:
                        int mon = mgrd[ pass_x ][ pass_y ];
                        if (mon != NON_MONSTER)
                        {
                            // one square, a few squares, anywhere...
                            if (!shift_monster(&menv[mon]) 
                                && !monster_blink(&menv[mon]))
                            {
                                monster_teleport( &menv[mon], true, true );
                            }
                        }

                        move_player_to_grid(pass_x, pass_y, false, true, true);
                        redraw_screen();
                    }
                }
                break; 

            case DELAY_BUTCHER:
                strcpy( info, "You finish " );
                strcat( info, (you.species == SP_TROLL
                                || you.species == SP_GHOUL) ? "ripping"
                                                            : "chopping" );

                strcat( info, " the corpse into pieces." );
                mpr( info );

                turn_corpse_into_chunks( mitm[ delay.parm1 ] );

                if (you.berserker && you.berserk_penalty != NO_BERSERK_PENALTY)
                {
                    mpr("You enjoyed that.");
                    you.berserk_penalty = 0;
                }
                break;

            case DELAY_DROP_ITEM:
                // Note:  checking if item is dropable is assumed to 
                // be done before setting up this delay... this includes
                // quantity (delay.parm2). -- bwr

                // Make sure item still exists.
                if (!is_valid_item( you.inv[ delay.parm1 ] ))
                    break;

                // Must handle unwield_item before we attempt to copy 
                // so that temporary brands and such are cleared. -- bwr
                if (delay.parm1 == you.equip[EQ_WEAPON])
                {   
                    unwield_item( delay.parm1 );
                    you.equip[EQ_WEAPON] = -1;
                    canned_msg( MSG_EMPTY_HANDED );
                }

                if (!copy_item_to_grid( you.inv[ delay.parm1 ], 
                                        you.x_pos, you.y_pos, delay.parm2 ))
                {
                    mpr("Too many items on this level, not dropping the item.");
                }
                else
                {
                    quant_name( you.inv[ delay.parm1 ], delay.parm2, 
                                DESC_NOCAP_A, str_pass );

                    snprintf( info, INFO_SIZE, "You drop %s.", str_pass );
                    mpr(info);

                    dec_inv_item_quantity( delay.parm1, delay.parm2 );
                }
                break;

            case DELAY_ASCENDING_STAIRS:
                up_stairs();
                untag_followers();
                break;

            case DELAY_DESCENDING_STAIRS:
                down_stairs( false, delay.parm1 );
                untag_followers();
                break;

            case DELAY_INTERUPTABLE:
            case DELAY_UNINTERUPTABLE:
                // these are simple delays that have no effect when complete
                break;

            default:
                mpr( "You finish doing something." );
                break; 
            }

            you.wield_change = true;
            print_stats();  // force redraw of the stats
            you.turn_is_over = true;
            you.delay_queue.pop();
        }
    }
}
