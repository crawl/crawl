/*
 *  File:       food.cc
 *  Summary:    Functions for eating and butchering.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <2>      5/20/99        BWR           Added CRAWL_PIZZA.
 *      <1>      -/--/--        LRH           Created
 */

#include "AppHdr.h"
#include "food.h"

#include <sstream>

#include <string.h>
// required for abs() {dlb}:
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "cio.h"
#include "clua.h"
#include "debug.h"
#include "delay.h"
#include "initfile.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "it_use2.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "mutation.h"
#include "output.h"
#include "player.h"
#include "religion.h"
#include "skills2.h"
#include "spells2.h"
#include "state.h"
#include "stuff.h"
#include "transfor.h"
#include "tutorial.h"
#include "xom.h"

static int   determine_chunk_effect(int which_chunk_type, bool rotten_chunk);
static void  eat_chunk( int chunk_effect, bool cannibal, int mon_intel = 0);
static void  eating(unsigned char item_class, int item_type);
static void  describe_food_change(int hunger_increment);
static bool  food_change(bool suppress_message);
static bool  vampire_consume_corpse(int mons_type, int mass,
                                    int chunk_type, bool rotten);
static void  heal_from_food(int hp_amt, int mp_amt, bool unrot,
                            bool restore_str);

/*
 **************************************************
 *                                                *
 *             BEGIN PUBLIC FUNCTIONS             *
 *                                                *
 **************************************************
*/

void make_hungry( int hunger_amount, bool suppress_msg )
{
    if (you.is_undead == US_UNDEAD)
        return;

#if DEBUG_DIAGNOSTICS
    set_redraw_status( REDRAW_HUNGER );
#endif

    you.hunger -= hunger_amount;

    if (you.hunger < 0)
        you.hunger = 0;

    // so we don't get two messages, ever.
    bool state_message = food_change(false);

    if (!suppress_msg && !state_message)
        describe_food_change( -hunger_amount );
}                               // end make_hungry()

void lessen_hunger( int satiated_amount, bool suppress_msg )
{
    if (you.is_undead == US_UNDEAD)
        return;

    you.hunger += satiated_amount;

    if (you.hunger > 12000)
        you.hunger = 12000;

    // so we don't get two messages, ever
    bool state_message = food_change(false);

    if (!suppress_msg && !state_message)
        describe_food_change(satiated_amount);
}                               // end lessen_hunger()

void set_hunger( int new_hunger_level, bool suppress_msg )
{
    if (you.is_undead == US_UNDEAD)
        return;

    int hunger_difference = (new_hunger_level - you.hunger);

    if (hunger_difference < 0)
        make_hungry( abs(hunger_difference), suppress_msg );
    else if (hunger_difference > 0)
        lessen_hunger( hunger_difference, suppress_msg );
}                               // end set_hunger()

// more of a "weapon_switch back from butchering" function, switching
// to a weapon is done using the wield_weapon code.
// special cases like staves of power or other special weps are taken
// care of by calling wield_effects()    {gdl}

void weapon_switch( int targ )
{
    if (targ == -1)
    {
        mpr( "You switch back to your bare hands." );
    }
    else
    {
        mprf("Switching back to %s.",
             you.inv[targ].name(DESC_INVENTORY).c_str());
    }

    // unwield the old weapon and wield the new.
    // XXX This is a pretty dangerous hack;  I don't like it.--GDL
    //
    // Well yeah, but that's because interacting with the wielding
    // code is a mess... this whole function's purpose was to 
    // isolate this hack until there's a proper way to do things. -- bwr
    if (you.equip[EQ_WEAPON] != -1)
        unwield_item(false);

    you.equip[EQ_WEAPON] = targ;

    // special checks: staves of power, etc
    if (targ != -1)
        wield_effects( targ, false );
}

// look for a butchering implement. If fallback is true,
// prompt the user if no obvious options exist.
// Returns whether a weapon was switched.
static bool find_butchering_implement( bool fallback )
{
    // If wielding a distortion weapon, never attempt to switch away
    // automatically.
    if (const item_def *wpn = you.weapon())
    {
        if (wpn->base_type == OBJ_WEAPONS
            && item_type_known(*wpn)
            && get_weapon_brand(*wpn) == SPWPN_DISTORTION)
        {
            mprf(MSGCH_WARN,
                 "You're wielding a weapon of distortion, will not autoswap "
                 "for butchering.");
            return false;
        }
    }
    
    // look for a butchering implement in your pack
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (is_valid_item( you.inv[i] )
            && can_cut_meat( you.inv[i] )
            && you.inv[i].base_type == OBJ_WEAPONS
            && item_known_uncursed(you.inv[i])
            && item_type_known(you.inv[i])
            && get_weapon_brand(you.inv[i]) != SPWPN_DISTORTION
            // don't even ask
            && !has_warning_inscription(you.inv[i], OPER_WIELD)
            && can_wield( &you.inv[i] ))
        {
            mpr("Switching to a butchering implement.");
            wield_weapon( true, i, false );
            return true;
        }
    }

    if ( !fallback )
        return false;

    // if we didn't swap above, then we still can't cut...let's call
    // wield_weapon() in the "prompt the user" way...
    
    // prompt for new weapon
    int old_weapon = you.equip[EQ_WEAPON];
    mpr( "What would you like to use?", MSGCH_PROMPT );
    wield_weapon( false );
    
    // let's see if the user did something...
    return (you.equip[EQ_WEAPON] != old_weapon);
}

bool butchery(int which_corpse)
{
    bool new_cursed = false;
    int old_weapon = you.equip[EQ_WEAPON];
    int old_gloves = you.equip[EQ_GLOVES];

    const transformation_type transform =
        static_cast<transformation_type>(you.attribute[ATTR_TRANSFORMATION]);
 
    // Xom probably likes this, occasionally
    bool teeth_butcher = (you.mutation[MUT_FANGS] == 3);

    bool barehand_butcher = (transform_can_butcher_barehanded(transform)
                             || you.has_claws()) && you.equip[EQ_GLOVES] == -1;

    bool gloved_butcher = you.has_claws() && (you.equip[EQ_GLOVES] != -1
                          && !item_cursed(you.inv[you.equip[EQ_GLOVES]]));

    bool can_butcher = teeth_butcher || barehand_butcher
                       || you.equip[EQ_WEAPON] != -1
                          && can_cut_meat(you.inv[you.equip[EQ_WEAPON]]);
    
    if (igrd[you.x_pos][you.y_pos] == NON_ITEM)
    {
        mpr("There isn't anything here!");
        return (false);
    }

    if (!Options.easy_butcher && !can_butcher)
    {
        mpr("Maybe you should try using a sharper implement.");
        return (false);
    }

    if (you.flight_mode() == FL_LEVITATE)
    {
        mpr("You can't reach the floor from up here.");
        return (false);
    }

    // It makes more sense that you first find out if there's anything
    // to butcher, *then* decide to actually butcher it.
    // The old code did it the other way.
    if ( !can_butcher && you.duration[DUR_BERSERKER] )
    {
        mpr ("You are too berserk to search for a butchering tool!");
        return (false);
    }

    // First determine how many things there are to butcher.
    int num_corpses = 0;
    int corpse_id   = -1;
    bool prechosen  = (which_corpse != -1);
    for (int o = igrd[you.x_pos][you.y_pos]; o != NON_ITEM; o = mitm[o].link)
    {
        if (mitm[o].base_type == OBJ_CORPSES &&
            mitm[o].sub_type == CORPSE_BODY)
        {
            corpse_id = o;
            num_corpses++;
            
            // return pre-chosen corpse if it exists
            if (prechosen && corpse_id == which_corpse)
                break;
        }
    }
    // pre-chosen corpse not found?
    if (prechosen && corpse_id != which_corpse)
        prechosen = false;

    bool canceled_butcher = false;
    
    // Now pick what you want to butcher. This is only a problem
    // if there are several corpses on the square.
    if ( num_corpses == 0 )
    {
        mpr("There isn't anything to dissect here.");
        return false;
    }
    else if ( !prechosen
              && (num_corpses > 1 || Options.always_confirm_butcher) )
    {
        corpse_id = -1;
        for (int o=igrd[you.x_pos][you.y_pos]; o != NON_ITEM; o = mitm[o].link)
        {
            if ( (mitm[o].base_type != OBJ_CORPSES) ||
                 (mitm[o].sub_type != CORPSE_BODY) )
                continue;
            
            // offer the possibility of butchering
            mprf("Butcher %s? [y/n/q/D]", mitm[o].name(DESC_NOCAP_A).c_str());
            // possible results:
            // 0 - cancel all butchery (quit)
            // 1 - say no to this butchery, continue prompting
            // 2 - OK this butchery
            // Yes, this is a hack because it's too annoying to adapt
            // yesnoquit() to this purpose.
            int result = 100;
            while (result == 100)
            {
                const int keyin = getchm(KC_CONFIRM);
                if (keyin == CK_ESCAPE || keyin == 'q' || keyin == 'Q')
                    result = 0;
                if (keyin == ' ' || keyin == '\r' || keyin == '\n' ||
                    keyin == 'n' || keyin == 'N')
                    result = 1;
                if (keyin == 'y' || keyin == 'Y' || keyin == 'd' ||
                    keyin == 'D')
                    result = 2;
            }

            if ( result == 0 )
            {
                canceled_butcher = true;
                corpse_id = -1;
                break;
            }
            else if ( result == 2 )
            {
                corpse_id = o;
                break;
            }
        }
    }
   
    // Do the actual butchery, if we found a good corpse.
    if ( corpse_id != -1 )
    {
        const bool can_sac = you.duration[DUR_PRAYER]
            && god_likes_butchery(you.religion);
        bool removed_gloves = false;
        bool wpn_switch = false;
        
        if ( Options.easy_butcher && !can_butcher )
        {
            // Try to find a butchering implement.
            // If you can butcher by taking off your gloves, don't prompt.
            wpn_switch = find_butchering_implement(!gloved_butcher);
            removed_gloves = gloved_butcher && !wpn_switch;
            if ( removed_gloves )
            {
                // Actually take off the gloves; this creates a
                // delay. We assume later on that gloves have a 1-turn
                // takeoff delay!
                takeoff_armour(old_gloves);
                barehand_butcher = true;
            }
            if ( wpn_switch )
            {
                new_cursed =
                    (you.weapon() != NULL) &&
                    (you.weapon()->base_type == OBJ_WEAPONS) &&
                    item_cursed( *you.weapon() );
            }
            
            // note that if barehanded then the user selected '-' when
            // switching weapons
            
            if (!barehand_butcher && (!wpn_switch ||
                                      you.weapon() == NULL ||
                                      !can_cut_meat(*you.weapon())))
            {
                // still can't butcher. Early out
                if ( you.weapon() == NULL )
                {
                    if (you.equip[EQ_GLOVES] == -1)
                        mpr("What, with your bare hands?");
                    else
                        mpr("Your gloves aren't that sharp!");
                }
                else
                    mpr("Maybe you should try using a sharper implement.");
                
                if ( !new_cursed && wpn_switch )
                    start_delay( DELAY_WEAPON_SWAP, 1, old_weapon );
                
                return false;                   
            }
            
            // switched to a good butchering knife
            can_butcher = true;
        }
        
        if ( can_butcher )
        {
            const bool rotten = food_is_rotten(mitm[corpse_id]);
            if (can_sac && !rotten)
                offer_corpse(corpse_id);
            else
            {
                if (can_sac && rotten)
                {
                    simple_god_message(coinflip() ?
                                       " refuses to accept that mouldy "
                                       "sacrifice!" :
                                       " demands fresh blood!", you.religion);
                }
                
                // If we didn't switch weapons, we get in one turn of butchery;
                // otherwise the work has to happen in the delay.
                if (!wpn_switch && !removed_gloves)
                    ++mitm[corpse_id].plus2;
                
                int work_req = 4 - mitm[corpse_id].plus2;
                if (work_req < 0)
                    work_req = 0;

                start_delay(DELAY_BUTCHER, work_req, corpse_id,
                            mitm[corpse_id].special);

                if (you.duration[DUR_PRAYER]
                    && god_hates_butchery(you.religion))
                {
                    did_god_conduct(DID_DEDICATED_BUTCHERY, 10);
                }
            }
        }

        // switch weapon back
        if (!new_cursed && wpn_switch)
            start_delay( DELAY_WEAPON_SWAP, 1, old_weapon );

        // put on the removed gloves
        if ( removed_gloves )
            start_delay( DELAY_ARMOUR_ON, 1, old_gloves );

        you.turn_is_over = true;    
        return true;
    }

    if (canceled_butcher)
        canned_msg(MSG_OK);
    else
        mpr("There isn't anything else to dissect here.");

    return false;
}                               // end butchery()

void lua_push_items(lua_State *ls, int link)
{
    lua_newtable(ls);
    int index = 0;
    for ( ; link != NON_ITEM; link = mitm[link].link)
    {
        lua_pushlightuserdata(ls, &mitm[link]);
        lua_rawseti(ls, -2, ++index);
    }
}

void lua_push_floor_items(lua_State *ls)
{
    lua_push_items(ls, igrd[you.x_pos][you.y_pos]);
}

void lua_push_inv_items(lua_State *ls = NULL)
{
    if (!ls)
        ls = clua.state();
    lua_newtable(ls);
    int index = 0;
    for (unsigned slot = 0; slot < ENDOFPACK; ++slot)
    {
        if (is_valid_item(you.inv[slot]))
        {
            lua_pushlightuserdata(ls, &you.inv[slot]);
            lua_rawseti(ls, -2, ++index);
        }
    }
}

static bool userdef_eat_food()
{
#ifdef CLUA_BINDINGS
    lua_push_floor_items(clua.state());
    lua_push_inv_items();
    bool ret = clua.callfn("c_eat", 2, 0);
    if (!ret && clua.error.length())
        mpr(clua.error.c_str());
    return ret;
#else
    return false;
#endif
}

bool prompt_eat_from_inventory(int slot)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return (false);
    }

    if (slot == -1 && you.species == SP_VAMPIRE)
    {
        slot =  prompt_invent_item( "Drain what?",
                                    MT_INVLIST,
                                    OSEL_VAMP_EAT,
                                    true, true, true, 0, NULL,
                                    OPER_EAT );
    }
        
    int which_inventory_slot = (slot != -1)? slot:
            prompt_invent_item(
                    "Eat which item?",
                    MT_INVLIST,
                    OBJ_FOOD,
                    true, true, true, 0, NULL,
                    OPER_EAT );
                    
    if (which_inventory_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return (false);
    }

    // this conditional can later be merged into food::can_ingest() when
    // expanded to handle more than just OBJ_FOOD 16mar200 {dlb}
    if (you.species != SP_VAMPIRE &&
        you.inv[which_inventory_slot].base_type != OBJ_FOOD)
    {
        mpr("You can't eat that!");
        return (false);
    }

    if (you.species == SP_VAMPIRE &&
       (you.inv[which_inventory_slot].base_type != OBJ_CORPSES
         || you.inv[which_inventory_slot].sub_type != CORPSE_BODY))
    {
        mpr("You crave blood!");
        return (false);
    }
       
    if (!can_ingest( you.inv[which_inventory_slot].base_type,
                        you.inv[which_inventory_slot].sub_type, false ))
    {
        return (false);
    }

    eat_from_inventory(which_inventory_slot);

    burden_change();
    you.turn_is_over = true;

    return (true);
}

// [ds] Returns true if something was eaten
bool eat_food(bool run_hook, int slot)
{
    if (you.is_undead == US_UNDEAD)
    {
        mpr("You can't eat.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    if (you.hunger >= 11000)
    {
        mpr("You're too full to eat anything.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    // If user hook ran, we don't know whether something
    // was eaten or not...
    if (run_hook && userdef_eat_food())
    {
        return (false);
    }

    if (igrd[you.x_pos][you.y_pos] != NON_ITEM && slot == -1)
    {
        const int res = eat_from_floor();
        if ( res == 1 )
            return true;
        if ( res == -1 )
            return false;
    }

    return (prompt_eat_from_inventory(slot));
}                               // end eat_food()

/*
 **************************************************
 *                                                *
 *              END PUBLIC FUNCTIONS              *
 *                                                *
 **************************************************
*/
static std::string how_hungry()
{
    if (you.hunger_state > HS_SATIATED)
        return ("full");
    else if (you.species == SP_VAMPIRE)
        return ("thirsty");
    return ("hungry");
}

static bool food_change(bool suppress_message)
{
    char newstate = HS_ENGORGED;
    bool state_changed = false;

    you.hunger = std::max(you_min_hunger(), you.hunger);
    you.hunger = std::min(you_max_hunger(), you.hunger);

    // get new hunger state
    if (you.hunger <= 1000)
        newstate = HS_STARVING;
    else if (you.hunger <= 1533)
        newstate = HS_NEAR_STARVING;
    else if (you.hunger <= 2066)
        newstate = HS_VERY_HUNGRY;
    else if (you.hunger <= 2600)
        newstate = HS_HUNGRY;
    else if (you.hunger < 7000)
        newstate = HS_SATIATED;
    else if (you.hunger < 9000)
        newstate = HS_FULL;
    else if (you.hunger < 11000)
        newstate = HS_VERY_FULL;

    if (newstate != you.hunger_state)
    {
        state_changed = true;
        you.hunger_state = newstate;
        set_redraw_status( REDRAW_HUNGER );

        if (newstate < HS_SATIATED)
            interrupt_activity( AI_HUNGRY );

        if (suppress_message == false)
        {
            switch (you.hunger_state)
            {
            case HS_STARVING:
                mpr("You are starving!", MSGCH_FOOD);
                learned_something_new(TUT_YOU_STARVING);
                you.check_awaken(500);
                break;
            case HS_NEAR_STARVING:
                mpr("You are near starving.", MSGCH_FOOD);
                learned_something_new(TUT_YOU_HUNGRY);
                break;
            case HS_VERY_HUNGRY:
                mprf(MSGCH_FOOD, "You are feeling very %s.", how_hungry().c_str());
                learned_something_new(TUT_YOU_HUNGRY);
                break;
            case HS_HUNGRY:
                mprf(MSGCH_FOOD, "You are feeling %s.", how_hungry().c_str());
                learned_something_new(TUT_YOU_HUNGRY);
                break;
            default:
                break;
            }
        }
    }

    return (state_changed);
}                               // end food_change()

// food_increment is positive for eating, negative for hungering
static void describe_food_change(int food_increment)
{
    int magnitude = (food_increment > 0)?food_increment:(-food_increment);
    std::string msg;

    if (magnitude == 0)
        return;

    if ( magnitude <= 100 )
        msg = "You feel slightly ";
    else if (magnitude <= 350)
        msg = "You feel somewhat ";
    else if (magnitude <= 800)
        msg = "You feel quite a bit ";
    else
        msg = "You feel a lot ";

    if ((you.hunger_state > HS_SATIATED) ^ (food_increment < 0))
        msg += "more ";
    else
        msg += "less ";

    msg += how_hungry().c_str();
    msg += ".";
    mpr(msg.c_str());
}                               // end describe_food_change()

static bool prompt_eat_chunk(const item_def &item, bool rotten)
{
    if (rotten && !you.mutation[MUT_SAPROVOROUS]
        && !yesno("Are you sure you want to eat this rotten meat?",
                  false, 'n'))
    {
        canned_msg(MSG_OK);
        return (false);
    }
    return (true);
}

// should really be merged into function below -- FIXME
void eat_from_inventory(int which_inventory_slot)
{
    item_def& food(you.inv[which_inventory_slot]);
    if (food.base_type == OBJ_CORPSES && food.sub_type == CORPSE_BODY)
    {
        if (you.species != SP_VAMPIRE)
            return;
            
        const int mons_type  = food.plus;
        const int chunk_type = mons_corpse_effect( mons_type );
        const bool rotten    = food_is_rotten(food);
        const int mass       = mons_weight(food.plus)/150;

        if (!vampire_consume_corpse(mons_type, mass, chunk_type, rotten))
            return;

        if (!mons_skeleton( mons_type ) || one_chance_in(4))
        {
            dec_inv_item_quantity( which_inventory_slot, 1 );
        }
        else
        {
            food.sub_type = CORPSE_SKELETON;
            food.special = 90;
            food.colour = LIGHTGREY;
        }
        return;
    }
    else if (food.sub_type == FOOD_CHUNK)
    {
        const int mons_type  = food.plus;
        const bool cannibal  = is_player_same_species(mons_type);
        const int intel      = mons_intel(mons_type) - I_ANIMAL;
        const int chunk_type = mons_corpse_effect( mons_type );
        const bool rotten    = food_is_rotten(food);

        if (!prompt_eat_chunk(food, rotten))
            return;

        eat_chunk(determine_chunk_effect(chunk_type, rotten), cannibal, intel);
    }
    else
        eating( food.base_type, food.sub_type );

    dec_inv_item_quantity( which_inventory_slot, 1 );
}                               // end eat_from_inventory()

void eat_floor_item(int item_link)
{
    item_def& food(mitm[item_link]);
    if (food.base_type == OBJ_CORPSES && food.sub_type == CORPSE_BODY)
    {
        const int mons_type  = food.plus;
        const int chunk_type = mons_corpse_effect( mons_type );
        const bool rotten    = food_is_rotten(food);
        const int mass       = mons_weight(food.plus)/150;

        if (!vampire_consume_corpse(mons_type, mass, chunk_type, rotten))
            return;

        if (!mons_skeleton( mons_type ) || one_chance_in(4))
        {
            dec_mitm_item_quantity( item_link, 1 );
        }
        else
        {
            food.sub_type = CORPSE_SKELETON;
            food.special  = 90;
            food.colour   = LIGHTGREY;
        }
        // dec_mitm_item_quantity( item_link, 1 );

        you.turn_is_over = 1;
        return;
    }
    else if (food.sub_type == FOOD_CHUNK)
    {
        const int chunk_type = mons_corpse_effect( food.plus );
        const int intel      = mons_intel( food.plus ) - I_ANIMAL;
        const bool cannibal  = is_player_same_species( food.plus );
        const bool rotten    = food_is_rotten(food);
        
        if (!prompt_eat_chunk(food, rotten))
            return;
            
        eat_chunk(determine_chunk_effect(chunk_type, rotten), cannibal, intel);
    }
    else
        eating( food.base_type, food.sub_type );

    you.turn_is_over = true;

    dec_mitm_item_quantity( item_link, 1 );
}

// return -1 for cancel, 1 for eaten, 0 for not eaten
int eat_from_floor()
{
    if (you.flight_mode() == FL_LEVITATE)
        return 0;

    bool need_more = false;
    for (int o = igrd[you.x_pos][you.y_pos]; o != NON_ITEM; o = mitm[o].link)
    {
        item_def& item = mitm[o];

        if (you.species != SP_VAMPIRE && item.base_type != OBJ_FOOD)
            continue;

        if (you.species == SP_VAMPIRE &&
            (item.base_type != OBJ_CORPSES || item.sub_type != CORPSE_BODY))
            continue;

        std::ostringstream prompt;
        prompt << (you.species == SP_VAMPIRE ? "Drink blood from" : "Eat")
               << ' ' << ((item.quantity > 1) ? "one of " : "")
               << item.name(DESC_NOCAP_A) << '?';
        const int ans = yesnoquit( prompt.str().c_str(), true, 0, false );
        if ( ans == -1 )        // quit
            return -1;
        else if ( ans == 1 )
        {
            if (can_ingest(item.base_type, item.sub_type, false))
            {
                eat_floor_item(o);
                return 1;
            }
            need_more = true;
        }
    }

    if (need_more && Options.auto_list)
        more();

    return (false);
}

static const char *chunk_flavour_phrase(bool likes_chunks)
{
    const char *phrase =
        likes_chunks? "tastes great." : "tastes terrible.";

    if (!likes_chunks)
    {
        const int gourmand = you.duration[DUR_GOURMAND];
        if (gourmand >= GOURMAND_MAX)
            phrase = 
                one_chance_in(1000)? "tastes like chicken!"
                                   : "tastes great.";
        else if (gourmand > GOURMAND_MAX * 75 / 100)
            phrase = "tastes very good.";
        else if (gourmand > GOURMAND_MAX * 50 / 100)
            phrase = "tastes good.";
        else if (gourmand > GOURMAND_MAX * 25 / 100)
            phrase = "is not very appetising.";
    }

    return (phrase);
}

void chunk_nutrition_message(int nutrition)
{
    int perc_nutrition = nutrition * 100 / CHUNK_BASE_NUTRITION;
    if (perc_nutrition < 15)
        mpr("That was extremely unsatisfying.");
    else if (perc_nutrition < 35)
        mpr("That was not very filling.");
}

static int apply_herbivore_chunk_effects(int nutrition)
{
    int how_herbivorous = you.mutation[MUT_HERBIVOROUS];

    while (how_herbivorous--)
        nutrition = nutrition * 80 / 100;

    return (nutrition);
}

static int chunk_nutrition(bool likes_chunks)
{
    int nutrition = CHUNK_BASE_NUTRITION;
    
    if (likes_chunks || you.hunger_state < HS_SATIATED)
    {
        return (likes_chunks? nutrition
                : apply_herbivore_chunk_effects(nutrition));
    }

    const int gourmand = 
        wearing_amulet(AMU_THE_GOURMAND)? you.duration[DUR_GOURMAND] : 0;

    int effective_nutrition = 
           nutrition * (gourmand + GOURMAND_NUTRITION_BASE)
                     / (GOURMAND_MAX + GOURMAND_NUTRITION_BASE);
    
#ifdef DEBUG_DIAGNOSTICS
    const int epercent = effective_nutrition * 100 / nutrition;
    mprf(MSGCH_DIAGNOSTICS, 
            "Gourmand factor: %d, chunk base: %d, effective: %d, %%: %d",
                    gourmand,
                    nutrition,
                    effective_nutrition,
                    epercent);
#endif

    return (apply_herbivore_chunk_effects(effective_nutrition));
}

static void say_chunk_flavour(bool likes_chunks)
{
    mprf("This raw flesh %s", chunk_flavour_phrase(likes_chunks));
}

// never called directly - chunk_effect values must pass
// through food::determine_chunk_effect() first {dlb}:
static void eat_chunk( int chunk_effect, bool cannibal, int mon_intel )
{

    bool likes_chunks = (you.omnivorous() ||
                         you.mutation[MUT_CARNIVOROUS]);
    int nutrition     = chunk_nutrition(likes_chunks);
    int hp_amt        = 0;
    bool suppress_msg = false; // do we display the chunk nutrition message?
    bool do_eat       = false;

    if (you.species == SP_GHOUL)
    {
        nutrition = CHUNK_BASE_NUTRITION;
        hp_amt = 1 + random2(5) + random2(1 + you.experience_level);
        suppress_msg = true;
    }

    switch (chunk_effect)
    {
    case CE_MUTAGEN_RANDOM:
        mpr("This meat tastes really weird.");
        mutate(RANDOM_MUTATION);
        did_god_conduct( DID_DELIBERATE_MUTATING, 10);
        xom_is_stimulated(100);
        break;

    case CE_MUTAGEN_BAD:
        mpr("This meat tastes *really* weird.");
        give_bad_mutation();
        did_god_conduct( DID_DELIBERATE_MUTATING, 10);
        xom_is_stimulated(random2(200));
        break;

    case CE_HCL:
        rot_player( 10 + random2(10) );
        if (disease_player( 50 + random2(100) ))
            xom_is_stimulated(random2(100));
        break;

    case CE_POISONOUS:
        mpr("Yeeuch - this meat is poisonous!");
        if (poison_player( 3 + random2(4) ))
            xom_is_stimulated(random2(128));
        break;

    case CE_ROTTEN:
    case CE_CONTAMINATED:
        if (you.mutation[MUT_SAPROVOROUS] == 3)
        {
            mprf("This %sflesh tastes delicious!",
                (chunk_effect == CE_ROTTEN) ? "rotting " : "");

            if (you.species == SP_GHOUL)
                heal_from_food(hp_amt, 0, !one_chance_in(4), one_chance_in(5));

            do_eat = true;
        }
        else
        {
            mpr("There is something wrong with this meat.");
            if (disease_player( 50 + random2(100) ))
                xom_is_stimulated(random2(100));
        }
        break;

    case CE_CLEAN:
    {
        if (you.mutation[MUT_SAPROVOROUS] == 3)
        {
            mpr("This raw flesh tastes good.");

            if (you.species == SP_GHOUL)
                heal_from_food((!one_chance_in(5)) ? hp_amt : 0, 0,
                    !one_chance_in(3), false);
        }
        else
            say_chunk_flavour(likes_chunks);

        do_eat = true;
        break;
    }
    }

    if (cannibal)
        did_god_conduct( DID_CANNIBALISM, 10 );
    else if (mon_intel > 0)
        did_god_conduct( DID_EAT_SOULED_BEING, mon_intel);
        
    if (do_eat)
    {
        start_delay( DELAY_EAT, 2, (suppress_msg) ? 0 : nutrition );
        lessen_hunger( nutrition, true );
    }

    return;
}                               // end eat_chunk()

static void eating(unsigned char item_class, int item_type)
{
    int temp_rand;              // probability determination {dlb}
    int food_value = 0;
    int how_herbivorous = you.mutation[MUT_HERBIVOROUS];
    int how_carnivorous = you.mutation[MUT_CARNIVOROUS];
    int carnivore_modifier = 0;
    int herbivore_modifier = 0;

    switch (item_class)
    {
    case OBJ_FOOD:
        // apply base sustenance {dlb}:
        switch (item_type)
        {
        case FOOD_MEAT_RATION:
        case FOOD_ROYAL_JELLY:
            food_value = 5000;
            break;
        case FOOD_BREAD_RATION:
            food_value = 4400;
            break;
        case FOOD_HONEYCOMB:
            food_value = 2000;
            break;
        case FOOD_SNOZZCUMBER:  // maybe a nasty side-effect from RD's book?
        case FOOD_PIZZA:
        case FOOD_BEEF_JERKY:
            food_value = 1500;
            break;
        case FOOD_CHEESE:
        case FOOD_SAUSAGE:
            food_value = 1200;
            break;
        case FOOD_ORANGE:
        case FOOD_BANANA:
        case FOOD_LEMON:
            food_value = 1000;
            break;
        case FOOD_PEAR:
        case FOOD_APPLE:
        case FOOD_APRICOT:
            food_value = 700;
            break;
        case FOOD_CHOKO:
        case FOOD_RAMBUTAN:
        case FOOD_LYCHEE:
            food_value = 600;
            break;
        case FOOD_STRAWBERRY:
            food_value = 200;
            break;
        case FOOD_GRAPE:
            food_value = 100;
            break;
        case FOOD_SULTANA:
            food_value = 70;     // will not save you from starvation
            break;
        default:
            break;
        }                       // end base sustenance listing {dlb}

        // next, sustenance modifier for carnivores/herbivores {dlb}:
        switch (item_type)
        {
        case FOOD_MEAT_RATION:
            carnivore_modifier = 500;
            herbivore_modifier = -1500;
            break;
        case FOOD_BEEF_JERKY:
        case FOOD_SAUSAGE:
            carnivore_modifier = 200;
            herbivore_modifier = -200;
            break;
        case FOOD_BREAD_RATION:
            carnivore_modifier = -1000;
            herbivore_modifier = 500;
            break;
        case FOOD_BANANA:
        case FOOD_ORANGE:
        case FOOD_LEMON:
            carnivore_modifier = -300;
            herbivore_modifier = 300;
            break;
        case FOOD_PEAR:
        case FOOD_APPLE:
        case FOOD_APRICOT:
        case FOOD_CHOKO:
        case FOOD_SNOZZCUMBER:
        case FOOD_RAMBUTAN:
        case FOOD_LYCHEE:
            carnivore_modifier = -200;
            herbivore_modifier = 200;
            break;
        case FOOD_STRAWBERRY:
            carnivore_modifier = -50;
            herbivore_modifier = 50;
            break;
        case FOOD_GRAPE:
        case FOOD_SULTANA:
            carnivore_modifier = -20;
            herbivore_modifier = 20;
            break;
        default:
            carnivore_modifier = 0;
            herbivore_modifier = 0;
            break;
        }                    // end carnivore/herbivore modifier listing {dlb}

        // next, let's take care of messaging {dlb}:
        if (how_carnivorous > 0 && carnivore_modifier < 0)
            mpr("Blech - you need meat!");
        else if (how_herbivorous > 0 && herbivore_modifier < 0)
            mpr("Blech - you need greens!");

        if (how_herbivorous < 1)
        {
            switch (item_type)
            {
            case FOOD_MEAT_RATION:
                mpr("That meat ration really hit the spot!");
                break;
            case FOOD_BEEF_JERKY:
                mprf("That beef jerky was %s!",
                     one_chance_in(4) ? "jerk-a-riffic"
                                      : "delicious");
                break;
            case FOOD_SAUSAGE:
                mpr("That sausage was delicious!");
                break;
            default:
                break;
            }
        }

        if (how_carnivorous < 1)
        {
            switch (item_type)
            {
            case FOOD_BREAD_RATION:
                mpr("That bread ration really hit the spot!");
                break;
            case FOOD_PEAR:
            case FOOD_APPLE:
            case FOOD_APRICOT:
                mprf("Mmmm... Yummy %s.",
                     (item_type == FOOD_APPLE)   ? "apple." :
                     (item_type == FOOD_PEAR)    ? "pear." :
                     (item_type == FOOD_APRICOT) ? "apricot."
                                                 : "fruit.");
                break;
            case FOOD_CHOKO:
                mpr("That choko was very bland.");
                break;
            case FOOD_SNOZZCUMBER:
                mpr("That snozzcumber tasted truly putrid!");
                break;
            case FOOD_ORANGE:
                mprf("That orange was delicious!%s",
                     one_chance_in(8) ? " Even the peel tasted good!" : "");
                break;
            case FOOD_BANANA:
                mprf("That banana was delicious!%s",
                     one_chance_in(8) ? " Even the peel tasted good!" : "");
                break;
            case FOOD_STRAWBERRY:
                mpr("That strawberry was delicious!");
                break;
            case FOOD_RAMBUTAN:
                mpr("That rambutan was delicious!");
                break;
            case FOOD_LEMON:
                mpr("That lemon was rather sour... But delicious nonetheless!");
                break;
            case FOOD_GRAPE:
                mpr("That grape was delicious!");
                break;
            case FOOD_SULTANA:
                mpr("That sultana was delicious! (but very small)");
                break;
            case FOOD_LYCHEE:
                mpr("That lychee was delicious!");
                break;
            default:
                break;
            }
        }

        switch (item_type)
        {
        case FOOD_HONEYCOMB:
            mpr("That honeycomb was delicious.");
            break;
        case FOOD_ROYAL_JELLY:
            mpr("That royal jelly was delicious!");
            restore_stat(STAT_ALL, false);
            break;
        case FOOD_PIZZA:
            if (!SysEnv.crawl_pizza.empty() && !one_chance_in(3))
                mprf("Mmm... %s.", SysEnv.crawl_pizza.c_str());
            else
            {
                if (how_carnivorous >= 1) // non-vegetable
                    temp_rand = 5 + random2(4);
                else if (how_herbivorous >= 1) // non-meaty
                    temp_rand = random2(6) + 2;
                else
                    temp_rand = random2(9);
                    
                mprf("Mmm... %s",
                     (temp_rand == 0) ? "Ham and pineapple." :
                     (temp_rand == 2) ? "Vegetable." :
                     (temp_rand == 3) ? "Pepperoni." :
                     (temp_rand == 4) ? "Yeuchh - Anchovies!" :
                     (temp_rand == 5) ? "Cheesy." :
                     (temp_rand == 6) ? "Supreme." :
                     (temp_rand == 7) ? "Super Supreme!"
                                      : "Chicken.");
            }
            break;
        case FOOD_CHEESE:
            temp_rand = random2(9);
            mprf("Mmm...%s.",
                 (temp_rand == 0) ? "Cheddar" :
                 (temp_rand == 1) ? "Edam" :
                 (temp_rand == 2) ? "Wensleydale" :
                 (temp_rand == 3) ? "Camembert" :
                 (temp_rand == 4) ? "Goat cheese" :
                 (temp_rand == 5) ? "Fruit cheese" :
                 (temp_rand == 6) ? "Mozzarella" :
                 (temp_rand == 7) ? "Sheep cheese"
                                  : "Yak cheese");
            break;
        default:
            break;
        }

        // finally, modify player's hunger level {dlb}:
        if (carnivore_modifier && how_carnivorous > 0)
            food_value += (carnivore_modifier * how_carnivorous);

        if (herbivore_modifier && how_herbivorous > 0)
            food_value += (herbivore_modifier * how_herbivorous);

        if (food_value > 0)
        {
            if (item_type == FOOD_MEAT_RATION || item_type == FOOD_BREAD_RATION)
                start_delay( DELAY_EAT, 3 );
            else
                start_delay( DELAY_EAT, 1 );

            lessen_hunger( food_value, true );
        }
        break;

    default:
        break;
    }

    return;
}                               // end eating()

bool can_ingest(int what_isit, int kindof_thing, bool suppress_msg, bool reqid,
                bool check_hunger)
{
    bool survey_says = false;

    // [ds] These redundant checks are now necessary - Lua might be calling us.
    if (you.is_undead == US_UNDEAD)
    {
        if (!suppress_msg)
            mpr("You can't eat.");
        return (false);
    }

    if (check_hunger && you.hunger >= 11000)
    {
        if (!suppress_msg)
            mpr("You're too full to eat anything.");
        return (false);
    }

    if (you.species == SP_VAMPIRE)
    {
        if (what_isit == OBJ_CORPSES && kindof_thing == CORPSE_BODY
            || what_isit == OBJ_POTIONS && kindof_thing == POT_BLOOD)
        {
            return (true);
        }
        else
        {
            if (!suppress_msg)
               mpr("Blech - you need blood!");
        }
        return (false);
    }

    bool ur_carnivorous = (you.mutation[MUT_CARNIVOROUS] == 3);

    bool ur_herbivorous = (you.mutation[MUT_HERBIVOROUS] == 3);

    // ur_chunkslover not defined in terms of ur_carnivorous because
    // a player could be one and not the other IMHO - 13mar2000 {dlb}
    bool ur_chunkslover = ( 
                (check_hunger? you.hunger_state <= HS_HUNGRY : true)
                           || wearing_amulet(AMU_THE_GOURMAND, !reqid)
                           || you.omnivorous()
                           || you.mutation[MUT_CARNIVOROUS]);

    switch (what_isit)
    {
    case OBJ_FOOD:
        if (you.species == SP_VAMPIRE)
        {
            if (!suppress_msg)
                mpr("Blech - you need blood!");
             return false;
        }
             
        switch (kindof_thing)
        {
        case FOOD_BREAD_RATION:
        case FOOD_PEAR:
        case FOOD_APPLE:
        case FOOD_CHOKO:
        case FOOD_SNOZZCUMBER:
        case FOOD_PIZZA:
        case FOOD_APRICOT:
        case FOOD_ORANGE:
        case FOOD_BANANA:
        case FOOD_STRAWBERRY:
        case FOOD_RAMBUTAN:
        case FOOD_LEMON:
        case FOOD_GRAPE:
        case FOOD_SULTANA:
        case FOOD_LYCHEE:
        case FOOD_CHEESE:
            if (ur_carnivorous)
            {
                survey_says = false;
                if (!suppress_msg)
                    mpr("Sorry, you're a carnivore.");
            }
            else
                survey_says = true;
            break;

        case FOOD_CHUNK:
            if (ur_herbivorous)
            {
                survey_says = false;
                if (!suppress_msg)
                    mpr("You can't eat raw meat!");
            }
            else if (!ur_chunkslover)
            {
                survey_says = false;
                if (!suppress_msg)
                    mpr("You aren't quite hungry enough to eat that!");
            }
            else
                survey_says = true;
            break;

        default:
            return (true);
        }
        break;

    case OBJ_CORPSES:
        if (you.species == SP_VAMPIRE)
        {
            if (kindof_thing == CORPSE_BODY)
                return true;
            else
            {
                if (!suppress_msg)
                    mpr("Blech - you need blood!");
                return false;
            }
        }
        return false;

    case OBJ_POTIONS: // called by lua
        if (get_ident_type(OBJ_POTIONS, kindof_thing) != ID_KNOWN_TYPE)
            return true;
        switch (kindof_thing)
        {
            case POT_BLOOD:
                if (ur_herbivorous)
                {
                    if (!suppress_msg)
                        mpr("Urks, you're a herbivore!");
                    return false;
                }
                return true;
            case POT_WATER:
                if (you.species == SP_VAMPIRE)
                {
                    if (!suppress_msg)
                        mpr("Blech - you need blood!");
                    return false;
                }
                return true;
             case POT_PORRIDGE:
                if (you.species == SP_VAMPIRE)
                {
                    if (!suppress_msg)
                        mpr("Blech - you need blood!");
                    return false;
                }
                else if (ur_carnivorous)
                {
                    if (!suppress_msg)
                        mpr("Sorry, you're a carnivore.");
                    return false;
                }
             default:
                return true;
        }

    // other object types are set to return false for now until
    // someone wants to recode the eating code to permit consumption
    // of things other than just food
    default:
        return (false);
    }

    return (survey_says);
}                               // end can_ingest()

// see if you can follow along here -- except for the Amulet of the Gourmand
// addition (long missing and requested), what follows is an expansion of how
// chunks were handled in the codebase up to this date -- I have never really
// understood why liches are hungry and not true undead beings ... {dlb}:
static int determine_chunk_effect(int which_chunk_type, bool rotten_chunk)
{
    int this_chunk_effect = which_chunk_type;

    // determine the initial effect of eating a particular chunk {dlb}:
    switch (this_chunk_effect)
    {
    case CE_HCL:
    case CE_MUTAGEN_RANDOM:
        if (you.species == SP_GHOUL
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH)
        {
            this_chunk_effect = CE_CLEAN;
        }
        break;

    case CE_POISONOUS:
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH
                || player_res_poison() > 0 && you.species != SP_VAMPIRE)
        {
            this_chunk_effect = CE_CLEAN;
        }
        break;

    case CE_CONTAMINATED:
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH
                && you.mutation[MUT_SAPROVOROUS] < 3)
        {
            this_chunk_effect = CE_CLEAN;
        }
        else
        {
            switch (you.mutation[MUT_SAPROVOROUS])
            {
            case 1:
                if (!one_chance_in(15))
                    this_chunk_effect = CE_CLEAN;
                break;

            case 2:
                if (!one_chance_in(45))
                    this_chunk_effect = CE_CLEAN;
                break;

            case 3:
                // Doing this here causes an odd message later. -- bwr
                // this_chunk_effect = CE_ROTTEN;
                break;

            default:
                if (!one_chance_in(3))
                    this_chunk_effect = CE_CLEAN;
                break;
            }
        }
        break;

    default:
        break;
    }

    // determine effects of rotting on base chunk effect {dlb}:
    if (rotten_chunk)
    {
        switch (this_chunk_effect)
        {
        case CE_CLEAN:
        case CE_CONTAMINATED:
            this_chunk_effect = CE_ROTTEN;
            break;
        case CE_MUTAGEN_RANDOM:
            this_chunk_effect = CE_MUTAGEN_BAD;
            break;
        default:
            break;
        }
    }

    // one last chance for some species to safely eat rotten food {dlb}:
    if (this_chunk_effect == CE_ROTTEN)
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH
                && you.mutation[MUT_SAPROVOROUS] < 3)
        {
            this_chunk_effect = CE_CLEAN;
        }
        else
        {
            switch (you.mutation[MUT_SAPROVOROUS])
            {
            case 1:
                if (!one_chance_in(5))
                    this_chunk_effect = CE_CLEAN;
                break;

            case 2:
                if (!one_chance_in(15))
                    this_chunk_effect = CE_CLEAN;
                break;

            default:
                break;
            }
        }
    }

    // The amulet of the gourmand will permit consumption of
    // contaminated meat as though it were "clean" meat - level 3
    // saprovores get rotting meat effect from clean chunks, since they
    // love rotting meat.
    if (wearing_amulet(AMU_THE_GOURMAND)
        && random2(GOURMAND_MAX) < you.duration[DUR_GOURMAND])
    {
        if (you.mutation[MUT_SAPROVOROUS] == 3)
        {
            // [dshaligram] Level 3 saprovores relish contaminated meat.
            if (this_chunk_effect == CE_CLEAN)
                this_chunk_effect = CE_CONTAMINATED;
        }
        else
        {
            // [dshaligram] New AotG behaviour - contaminated chunks become
            // clean, but rotten chunks remain rotten.
            if (this_chunk_effect == CE_CONTAMINATED)
                this_chunk_effect = CE_CLEAN;
        }
    }

    return (this_chunk_effect);
}                               // end determine_chunk_effect()

static bool vampire_consume_corpse(const int mons_type, int max_chunks,
                                   const int chunk_type, const bool rotten)
{
    if (chunk_type == CE_HCL)
    {
        mpr( "There is no blood in this body!" );
        return false;
    }

    // This is the exact formula of corpse nutrition for chunk lovers
    max_chunks = 1 + random2(max_chunks);
    int mass = CHUNK_BASE_NUTRITION * max_chunks;
    int food_value = 0, hp_amt = 0, mp_amt = 0;

    if (rotten)
    {
        if (wearing_amulet(AMU_THE_GOURMAND))
        {
            food_value = mass/2 + random2(you.experience_level * 5);
            mpr("Slurp.");
            did_god_conduct(DID_DRINK_BLOOD, 8);
        }
        else
        {
            mpr("It's not fresh enough.");
            return false;
        }
   }
   else
   {
        hp_amt++;

        switch (mons_type)
        {
           case MONS_HUMAN:
               food_value = mass + random2avg(you.experience_level * 10, 2);
               mpr( "This warm blood tastes really delicious!" );
               hp_amt += 1 + random2(1 + you.experience_level);
               break;

           case MONS_ELF:
               food_value = mass + random2avg(you.experience_level * 10, 2);
               mpr( "This warm blood tastes magically delicious!" );
               mp_amt += 1 + random2(3);
               break;

           default:
               switch (chunk_type)
               {
                  case CE_CLEAN:
                      food_value = mass;
                      mpr( "This warm blood tastes delicious!" );
                      break;
                  case CE_CONTAMINATED:
                      food_value = mass / (random2(3) + 1);
                      mpr( "Somehow that blood was not very filling!" );
                      break;
                  case CE_POISONOUS:
                      food_value = random2(mass) - mass/2;
                      mpr( "Blech - that blood tastes nasty!" );
                      break;
                  case CE_MUTAGEN_RANDOM:
                      food_value = random2(mass);
                      mpr( "That blood tastes really weird!" );
                      mutate(RANDOM_MUTATION);
                      xom_is_stimulated(100);
                      break;
               }
        }
        did_god_conduct(DID_DRINK_BLOOD, 8);
    }

    heal_from_food(hp_amt, mp_amt,
                   !rotten && one_chance_in(4), one_chance_in(3));

    lessen_hunger( food_value, true );
    describe_food_change(food_value);

    // The delay for eating a chunk (mass 1000) is 2
    // Here the base nutrition value equals that of chunks,
    // but the delay should be greater.
    start_delay( DELAY_EAT, mass / 400 ); 
    return true;
} // end vampire_consume_corpse()

static void heal_from_food(int hp_amt, int mp_amt, bool unrot,
                           bool restore_str)
{
    if (hp_amt > 0)
        inc_hp(hp_amt, false);

    if (mp_amt > 0)
        inc_mp(mp_amt, false);

    if (unrot && player_rotted())
    {
        mpr("You feel more resilient.");
        unrot_hp(1);
    }

    if (restore_str && you.strength < you.max_strength)
    {
        mpr("You feel your strength returning.");
        you.strength++;
        you.redraw_strength = true;
        burden_change();
    }

    calc_hp();
    calc_mp();
} // end heal_from_food()

int you_max_hunger()
{
    // this case shouldn't actually happen:
    if (you.is_undead == US_UNDEAD)
        return 6000;

    // take care of ghouls - they can never be 'full'
    if (you.species == SP_GHOUL)
        return 6999;

    return 40000;
}

int you_min_hunger()
{
    // this case shouldn't actually happen:
    if (you.is_undead == US_UNDEAD)
        return 6000;

    // vampires can never starve to death
    if (you.species == SP_VAMPIRE)
        return 701;

    return 0;
}
