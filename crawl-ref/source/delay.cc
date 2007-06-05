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

#include "clua.h"
#include "command.h"
#include "delay.h"
#include "enum.h"
#include "food.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "it_use2.h"
#include "message.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "spl-util.h"
#include "stuff.h"
#include "travel.h"
#include "tutorial.h"

extern std::vector<SelItem> items_for_multidrop;

static void armour_wear_effects(const int item_inv_slot);
static void handle_run_delays(const delay_queue_item &delay);
static void handle_macro_delay();
static void finish_delay(const delay_queue_item &delay);

// Returns true if this delay can act as a parent to other delays, i.e. if
// other delays can be spawned while this delay is running. If is_parent_delay
// returns true, new delays will be pushed immediately to the front of the
// delay in question, rather than at the end of the queue.
static bool is_parent_delay(int delay)
{
    // Interlevel travel can do upstairs/downstairs delays.
    // Lua macros can in theory perform any of the other delays,
    // including travel; in practise travel still assumes there can be
    // no parent delay.
    return (delay == DELAY_TRAVEL
            || delay == DELAY_MACRO
            || delay == DELAY_MULTIDROP);
}

static int push_delay(const delay_queue_item &delay)
{
    for (delay_queue_type::iterator i = you.delay_queue.begin(); 
            i != you.delay_queue.end();
            ++i)
    {
        if (is_parent_delay( i->type ))
        {
            you.delay_queue.insert(i, delay);
            return (i - you.delay_queue.begin());
        }
    }
    you.delay_queue.push_back( delay );
    return (you.delay_queue.size() - 1);
}

static void pop_delay()
{
    if (!you.delay_queue.empty())
        you.delay_queue.erase( you.delay_queue.begin() );
}

static void clear_pending_delays()
{
    while (you.delay_queue.size() > 1)
    {
        const delay_queue_item delay =
            you.delay_queue[you.delay_queue.size() - 1];
        you.delay_queue.pop_back();
        
        if (is_run_delay(delay.type) && you.running)
            stop_running();
    }
}

void start_delay( int type, int turns, int parm1, int parm2 )
/***********************************************************/
{
    delay_queue_item delay;
    
    delay.type = type;
    delay.duration = turns;
    delay.parm1 = parm1;
    delay.parm2 = parm2;

    // Handle zero-turn delays (possible with butchering).
    if (turns == 0)
    {
        // Don't issue startup message.
        if (push_delay(delay) == 0)
            finish_delay(delay);
        return;
    }

    switch ( delay.type )
    {
    case DELAY_ARMOUR_ON:
        mpr("You start putting on your armour.", MSGCH_MULTITURN_ACTION);
        break;
    case DELAY_ARMOUR_OFF:
        mpr("You start removing your armour.", MSGCH_MULTITURN_ACTION);
        break;
    case DELAY_MEMORISE:
        mpr("You start memorising the spell.", MSGCH_MULTITURN_ACTION);
        break;
    case DELAY_PASSWALL:
        mpr("You begin to meditate on the wall.", MSGCH_MULTITURN_ACTION);
        break;
    default:
        break;
    }
    push_delay( delay ); 
}

void stop_delay( void )
/*********************/
{
    if ( you.delay_queue.empty() )
        return;

    delay_queue_item delay = you.delay_queue.front(); 

    const bool butcher_swap_warn =
        delay.type == DELAY_BUTCHER
        && (you.delay_queue.size() >= 2
            && you.delay_queue[1].type == DELAY_WEAPON_SWAP);
    
    // At the very least we can remove any queued delays, right 
    // now there is no problem with doing this... note that
    // any queuing here can only happen from a single command,
    // as the effect of a delay doesn't normally allow interaction
    // until it is done... it merely chains up individual actions
    // into a single action.  -- bwr
    clear_pending_delays();

    switch (delay.type)
    {
    case DELAY_BUTCHER:
        // Corpse keeps track of work in plus2 field, see handle_delay() -- bwr
        if (butcher_swap_warn)
            mpr("You stop butchering the corpse; not switching back to "
                "primary weapon.",
                MSGCH_WARN);
        else
            mpr( "You stop butchering the corpse." );
        pop_delay();
        break;

    case DELAY_MEMORISE:
        // Losing work here is okay... having to start from 
        // scratch is a reasonable behaviour. -- bwr
        mpr( "Your memorisation is interrupted." );
        pop_delay();
        break;

    case DELAY_PASSWALL:        
        // The lost work here is okay since this spell requires 
        // the player to "attune to the rock".  If changed, then
        // the delay should be increased to reduce the power of
        // this spell. -- bwr
        mpr( "Your meditation is interrupted." );
        pop_delay();
        break;

    case DELAY_MULTIDROP:
        // No work lost
        if (!items_for_multidrop.empty())
            mpr( "You stop dropping stuff." );
        pop_delay();
        break;

    case DELAY_RUN:
    case DELAY_REST:
    case DELAY_TRAVEL:
    case DELAY_MACRO:
        // Always interruptible.
        pop_delay();

        // Keep things consistent, otherwise disturbing phenomena can occur.
        // Note that runrest::stop() will turn around and call stop_delay()
        // again, but that's okay because the delay is already popped off 
        // the queue.
        if (is_run_delay(delay.type) && you.running)
            stop_running();

        // There's no special action needed for macros - if we don't call out
        // to the Lua function, it can't do damage.
        break;

    case DELAY_INTERRUPTIBLE:  
        // always stoppable by definition... 
        // try using a more specific type anyways. -- bwr
        pop_delay();
        break;

    case DELAY_EAT:
        // XXX: Large problems with object destruction here... food can
        // be from in the inventory or on the ground and these are
        // still handled quite differently.  Eventually we would like 
        // this to be stoppable, with partial food items implemented. -- bwr
        break; 

    case DELAY_ARMOUR_ON:
    case DELAY_ARMOUR_OFF:
        // These two have the default action of not being interruptible,
        // although they will often be chained (remove cloak, remove 
        // armour, wear new armour, replace cloak), all of which can
        // be stopped when complete.  This is a fairly reasonable 
        // behaviour, although perhaps the character should have 
        // option of reversing the current action if it would take 
        // less time to get out of the plate mail that's half on
        // than it would take to continue.  Probably too much trouble,
        // and would have to have a prompt... this works just fine. -- bwr
        break;

    case DELAY_WEAPON_SWAP:       // one turn... too much trouble 
    case DELAY_DROP_ITEM:         // one turn... only used for easy armour drops
    case DELAY_ASCENDING_STAIRS:  // short... and probably what people want
    case DELAY_DESCENDING_STAIRS: // short... and probably what people want
    case DELAY_UNINTERRUPTIBLE:   // never stoppable 
    case DELAY_JEWELLERY_ON:      // one turn
    default:
        break;
    }

    if (is_run_delay(delay.type))
        update_turn_count();
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

bool is_run_delay(int delay)
{
    return (delay == DELAY_RUN || delay == DELAY_REST 
            || delay == DELAY_TRAVEL);
}

bool is_being_butchered(const item_def &item)
{
    if (!you_are_delayed())
        return (false);

    const delay_queue_item &delay = you.delay_queue.front();
    if (delay.type == DELAY_BUTCHER)
    {
        const item_def &corpse = mitm[ delay.parm1 ];
        return (&corpse == &item);
    }

    return (false);
}

void handle_delay( void )
/***********************/
{
    char  str_pass[ ITEMNAME_SIZE ];

    if (!you_are_delayed())
        return;

    delay_queue_item &delay = you.delay_queue.front();

    // Run delays and Lua delays don't have a specific end time.
    if (is_run_delay(delay.type))
    {
        // Hack - allow autoprayer to trigger during run delays
        if ( do_autopray() )
            return;
            
        handle_run_delays(delay);
        return;
    }

    if (delay.type == DELAY_MACRO)
    {
        handle_macro_delay();
        return;
    }

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
                 (delay.parm2 >= 100) )
            {
                mpr("The corpse rots.", MSGCH_ROTTEN_MEAT);
                delay.parm2 = 99; // don't give the message twice
            }

            // mark work done on the corpse in case we stop -- bwr
            mitm[ delay.parm1 ].plus2++;
        }
        else
        {
            // corpse is no longer valid! End the butchering normally
            // instead of using stop_delay() so that the player switches
            // back to their main weapon if necessary.
            delay.duration = 0;
        }
    }
    if ( delay.type == DELAY_MULTIDROP )
    {

        // Throw away invalid items; items usually go invalid because
        // of chunks rotting away.
        while (!items_for_multidrop.empty() 
               // Don't look for gold in inventory
               && items_for_multidrop[0].slot != PROMPT_GOT_SPECIAL
               && !is_valid_item(you.inv[ items_for_multidrop[0].slot ]))
            items_for_multidrop.erase( items_for_multidrop.begin() );

        if ( items_for_multidrop.empty() )
        {
            // ran out of things to drop
            pop_delay();
            return;
        }
    }

    // Handle delay:
    if (delay.duration > 0)
    {
#if DEBUG_DIAGNOSTICS
        snprintf( info, INFO_SIZE, "Delay type: %d (%s), duration: %d", 
                  delay.type, delay_name(delay.type), delay.duration ); 

        mpr( info, MSGCH_DIAGNOSTICS );
#endif
        // delay.duration-- *must* be done before multidrop, because
        // multidrop is now a parent delay, which means other delays
        // can be pushed to the front of the queue, invalidating the
        // "delay" reference here, and resulting in tons of debugging
        // fun with valgrind.
        delay.duration--;
        
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
        case DELAY_MULTIDROP:
            drop_item( items_for_multidrop[0].slot,
                       items_for_multidrop[0].quantity,
                       items_for_multidrop.size() == 1 );
            items_for_multidrop.erase( items_for_multidrop.begin() );
            break;
        default:
            break;
        }
    }
    else 
    {
        finish_delay(delay);
    }
}

static void finish_delay(const delay_queue_item &delay)
{
    char  str_pass[ ITEMNAME_SIZE ];
    switch (delay.type)
    {
    case DELAY_WEAPON_SWAP:
        weapon_switch( delay.parm1 );
        break;

    case DELAY_JEWELLERY_ON:
        puton_ring( delay.parm1, false );
        break;

    case DELAY_ARMOUR_ON:
        armour_wear_effects( delay.parm1 );
        break;

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
        // For chunks, warn the player if they're not getting much
        // nutrition.
        if (delay.parm1)
            chunk_nutrition_message(delay.parm1);
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
        break; 
    }

    case DELAY_BUTCHER:
    {
        const item_def &item = mitm[delay.parm1];
        if (is_valid_item(item) && item.base_type == OBJ_CORPSES)
        {
            mprf("You finish %s the corpse into pieces.",
                 (you.species==SP_TROLL || you.species == SP_GHOUL) ? "ripping"
                 : "chopping");

            turn_corpse_into_chunks( mitm[ delay.parm1 ] );

            if (you.berserker && you.berserk_penalty != NO_BERSERK_PENALTY)
            {
                mpr("You enjoyed that.");
                you.berserk_penalty = 0;
            }
        }
        else
        {
            mpr("You stop butchering the corpse.");
        }
        break;
    }

    case DELAY_DROP_ITEM:
        // Note:  checking if item is droppable is assumed to 
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
                                you.x_pos, you.y_pos, delay.parm2,
                                true ))
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

    case DELAY_INTERRUPTIBLE:
    case DELAY_UNINTERRUPTIBLE:
        // these are simple delays that have no effect when complete
        break;

    default:
        mpr( "You finish doing something." );
        break; 
    }

    you.wield_change = true;
    print_stats();  // force redraw of the stats
    pop_delay();

    // Chain onto the next delay.
    handle_delay();
}

static void armour_wear_effects(const int item_slot)
{
    item_def &arm = you.inv[item_slot];

    set_ident_flags(arm, ISFLAG_EQ_ARMOUR_MASK );
    mprf("You finish putting on %s.", item_name(arm, DESC_NOCAP_YOUR));

    const equipment_type eq_slot = get_armour_slot(arm);

    if (eq_slot == EQ_BODY_ARMOUR)
    {
        you.equip[EQ_BODY_ARMOUR] = item_slot;

        if (you.duration[DUR_ICY_ARMOUR] != 0)
        {
            mpr( "Your icy armour melts away.", MSGCH_DURATION );
            you.redraw_armour_class = 1;
            you.duration[DUR_ICY_ARMOUR] = 0;
        }
    }
    else
    {
        switch (eq_slot)
        {
        case EQ_SHIELD:
            if (you.duration[DUR_CONDENSATION_SHIELD])
            {
                mpr( "Your icy shield evaporates.", MSGCH_DURATION );
                you.duration[DUR_CONDENSATION_SHIELD] = 0;
            }
            you.equip[EQ_SHIELD] = item_slot;
            break;
        case EQ_CLOAK:
            you.equip[EQ_CLOAK] = item_slot;
            break;
        case EQ_HELMET:
            you.equip[EQ_HELMET] = item_slot;
            break;
        case EQ_GLOVES:
            you.equip[EQ_GLOVES] = item_slot;
            break;
        case EQ_BOOTS:
            you.equip[EQ_BOOTS] = item_slot;
            break;
        default:
            break;
        }
    }

    int ego = get_armour_ego_type( arm );
    if (ego != SPARM_NORMAL)
    {   
        switch (ego)
        {
        case SPARM_RUNNING:
            mprf("You feel quick%s.",
                    (you.species == SP_NAGA || you.species == SP_CENTAUR) 
                    ? "" : " on your feet");
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

    if (is_random_artefact( arm ))
        use_randart( item_slot );

    if (item_cursed( arm )) {
        mpr( "Oops, that feels deathly cold." );
        learned_something_new(TUT_YOU_CURSED);
    }

    if (eq_slot == EQ_SHIELD)
        warn_shield_penalties();

    you.redraw_armour_class = 1;
    you.redraw_evasion = 1;
}

static command_type get_running_command()
{
    if ( kbhit() )
    {
        stop_running();
        return CMD_NO_CMD;
    }
    if ( is_resting() )
    {
        you.running.rest();
        if ( !is_resting() && you.running.hp == you.hp
             && you.running.mp == you.magic_points )
        {
            mpr("Done searching.");
        }
        return CMD_MOVE_NOWHERE;
    }
    return direction_to_command( you.running.x, you.running.y );
}

static void handle_run_delays(const delay_queue_item &delay)
{
    // Handle inconsistencies between the delay queue and you.running.
    // We don't want to send the game into a deadlock.
    if (!you.running)
    {
        update_turn_count();
        pop_delay();
        return;
    }

    if (you.turn_is_over)
        return;

    command_type cmd = CMD_NO_CMD;
    switch (delay.type)
    {
    case DELAY_REST:
    case DELAY_RUN:
        cmd = get_running_command();
        break;
    case DELAY_TRAVEL:
        cmd = travel();
        break;
    }

    if (cmd != CMD_NO_CMD)
    {
        if ( delay.type != DELAY_REST )
            mesclr();
        process_command(cmd);
    }

    // If you.running has gone to zero, and the run delay was not
    // removed, remove it now. This is needed to clean up after
    // find_travel_pos() function in travel.cc.
    if (!you.running && is_run_delay(current_delay_action()))
    {
        pop_delay();
        update_turn_count();
    }

    if (you.running && !you.turn_is_over
        && you_are_delayed()
        && !is_run_delay(current_delay_action()))
    {
        handle_delay();
    }
}

static void handle_macro_delay()
{
    run_macro();
}

void run_macro(const char *macroname)
{
    const int currdelay = current_delay_action();
    if (currdelay != DELAY_NOT_DELAYED && currdelay != DELAY_MACRO)
        return;

#ifdef CLUA_BINDINGS
    if (!clua)
    {
        mpr("Lua not initialized", MSGCH_DIAGNOSTICS);
        stop_delay();
        return;
    }

    if (!clua.callbooleanfn(false, "c_macro", "s", macroname))
    {
        if (clua.error.length())
            mpr(clua.error.c_str());

        stop_delay();
    }
    else
    {
        start_delay(DELAY_MACRO, 1);
    }
#else
    stop_delay();
#endif
}

// Returns true if the delay should be interrupted, false if the user function
// had no opinion on the matter, -1 if the delay should not be interrupted.
static int userdef_interrupt_activity( const delay_queue_item &idelay,
                                        activity_interrupt_type ai, 
                                        const activity_interrupt_data &at )
{
#ifdef CLUA_BINDINGS
    const int delay = idelay.type;
    lua_State *ls = clua.state();
    if (!ls || ai == AI_FORCE_INTERRUPT)
        return (true);

    // Kludge: We have to continue to support ch_stop_run. :-(
    if (is_run_delay(delay) && you.running && ai == AI_SEE_MONSTER)
    {
        bool stop_run = false;
        if (clua.callfn("ch_stop_run", "M>b",
                        (const monsters *) at.data, &stop_run))
        {
            if (stop_run)
                return (true);

            // No further processing.
            return (-1);
        }

        // If we get here, ch_stop_run wasn't defined, fall through to the
        // other handlers.
    }
    
    const char *interrupt_name = activity_interrupt_name(ai);
    const char *act_name = delay_name(delay);

    bool ran = clua.callfn("c_interrupt_activity", "1:ssA",
                    act_name, interrupt_name, &at);
    if (ran)
    {
        // If the function returned nil, we want to cease processing.
        if (lua_isnil(ls, -1))
        {
            lua_pop(ls, 1);
            return (-1);
        }

        bool stopact = lua_toboolean(ls, -1);
        lua_pop(ls, 1);
        if (stopact)
            return (true);
    }

    if (delay == DELAY_MACRO &&
                clua.callbooleanfn(true, "c_interrupt_macro",
                    "sA", interrupt_name, &at))
        return (true);

#endif
    return (false);
}

// Returns true if the activity should be interrupted, false otherwise.
static bool should_stop_activity(const delay_queue_item &item,
                                 activity_interrupt_type ai,
                                 const activity_interrupt_data &at)
{
    int userd = userdef_interrupt_activity(item, ai, at);

    // If the user script wanted to stop the activity or cease processing,
    // do so.
    if (userd)
        return (userd == 1);

    return (ai == AI_FORCE_INTERRUPT || 
            (Options.activity_interrupts[item.type][ai]));
}

inline static void monster_warning(activity_interrupt_type ai,
                                   const activity_interrupt_data &at,
                                   int atype)
{
    if ( ai == AI_SEE_MONSTER && is_run_delay(atype) )
    {
        const monsters* mon = static_cast<const monsters*>(at.data);
#ifndef DEBUG_DIAGNOSTICS
        mprf(MSGCH_WARN, "%s comes into view.", ptr_monam(mon, DESC_CAP_A));
#else
        formatted_string fs( channel_to_colour(MSGCH_WARN) );
        fs.cprintf("%s (", ptr_monam(mon, DESC_PLAIN));
        fs.add_glyph( mon );
        fs.cprintf(") in view: (%d,%d), see_grid: %s",
             mon->x, mon->y,
             see_grid(mon->x, mon->y)? "yes" : "no");
        formatted_mpr(fs, MSGCH_WARN);
#endif
    }
}

static void paranoid_option_disable( activity_interrupt_type ai,
                                     const activity_interrupt_data &at )
{
    if (ai == AI_HIT_MONSTER || ai == AI_MONSTER_ATTACKS)
    {
        const monsters* mon = static_cast<const monsters*>(at.data);
        if (mon && !player_monster_visible(mon))
        {
            std::vector<std::string> deactivatees;
            if (Options.autoprayer_on)
            {
                deactivatees.push_back("autoprayer");
                Options.autoprayer_on = false;
            }

            if (Options.autopickup_on && Options.safe_autopickup)
            {
                deactivatees.push_back("autopickup");
                Options.autopickup_on = false;
            }

            if (!deactivatees.empty())
                mprf(MSGCH_WARN, "Deactivating %s.",
                      comma_separated_line(deactivatees.begin(),
                                           deactivatees.end()).c_str());
        }
    }
}

// Returns true if any activity was stopped.
bool interrupt_activity( activity_interrupt_type ai, 
                         const activity_interrupt_data &at )
{
    paranoid_option_disable(ai, at);
    
    const int delay = current_delay_action();
    
    if (delay == DELAY_NOT_DELAYED)
        return (false);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Activity interrupt: %s",
         activity_interrupt_name(ai));
#endif
    
    // First try to stop the current delay.
    const delay_queue_item &item = you.delay_queue.front();

    if (should_stop_activity(item, ai, at))
    {
        monster_warning(ai, at, item.type);
        stop_delay();
        return (true);
    }

    // Check the other queued delays; the first delay that is interruptible
    // will kill itself and all subsequent delays. This is so that a travel
    // delay stacked behind a delay such as stair/autopickup will be killed
    // correctly by interrupts that the simple stair/autopickup delay ignores.
    for (int i = 1, size = you.delay_queue.size(); i < size; ++i)
    {
        const delay_queue_item &it = you.delay_queue[i];
        if (should_stop_activity(it, ai, at))
        {
            // Do we have a queued run delay? If we do, flush the delay queue
            // so that stop running Lua notifications happen.
            for (int j = i; j < size; ++j)
            {
                if (is_run_delay( you.delay_queue[j].type ))
                {
                    monster_warning(ai, at, you.delay_queue[j].type);
                    stop_delay();
                    return (true);
                }
            }

            // Non-run queued delays can be discarded without any processing.
            you.delay_queue.erase( you.delay_queue.begin() + i,
                                   you.delay_queue.end() );
            return (true);
        }
    }

    return (false);
}

static const char *activity_interrupt_names[] =
{
    "force", "keypress", "full_hp", "full_mp", "statue",
    "hungry", "message", "hp_loss", "burden", "stat",
    "monster", "monster_attack", "teleport", "hit_monster"
};

const char *activity_interrupt_name(activity_interrupt_type ai)
{
    ASSERT( sizeof(activity_interrupt_names) 
            / sizeof(*activity_interrupt_names) == NUM_AINTERRUPTS );

    if (ai == NUM_AINTERRUPTS)
        return ("");

    return activity_interrupt_names[ai];
}

activity_interrupt_type get_activity_interrupt(const std::string &name)
{
    ASSERT( sizeof(activity_interrupt_names) 
            / sizeof(*activity_interrupt_names) == NUM_AINTERRUPTS );

    for (int i = 0; i < NUM_AINTERRUPTS; ++i)
        if (name == activity_interrupt_names[i])
            return activity_interrupt_type(i);

    return (NUM_AINTERRUPTS);
}

static const char *delay_names[] =
{
    "not_delayed", "eat", "armour_on", "armour_off", "jewellery_on",
    "memorise", "butcher", "weapon_swap", "passwall",
    "drop_item", "multidrop", "ascending_stairs", "descending_stairs", "run",
    "rest", "travel", "macro", "interruptible", "uninterruptible",
};

// Gets a delay given its name.
// name must be lowercased already!
delay_type get_delay(const std::string &name)
{
    ASSERT( sizeof(delay_names) / sizeof(*delay_names) == NUM_DELAYS );

    for (int i = 0; i < NUM_DELAYS; ++i)
    {
        if (name == delay_names[i])
            return delay_type(i);
    }

    // Also check American spellings:
    if (name == "armor_on")
        return (DELAY_ARMOUR_ON);

    if (name == "armor_off")
        return (DELAY_ARMOUR_OFF);

    if (name == "memorize")
        return (DELAY_MEMORISE);

    if (name == "jewelry_on")
        return (DELAY_JEWELLERY_ON);

    return (NUM_DELAYS);
}

const char *delay_name(int delay)
{
    ASSERT( sizeof(delay_names) / sizeof(*delay_names) == NUM_DELAYS );

    if (delay < 0 || delay >= NUM_DELAYS)
        return ("");

    return delay_names[delay];
}
