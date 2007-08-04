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

#include <string.h>
// required for abs() {dlb}:
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "clua.h"
#include "debug.h"
#include "delay.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "it_use2.h"
#include "macro.h"
#include "misc.h"
#include "mon-util.h"
#include "mutation.h"
#include "player.h"
#include "religion.h"
#include "skills2.h"
#include "spells2.h"
#include "stuff.h"
#include "transfor.h"
#include "tutorial.h"

static int   determine_chunk_effect(int which_chunk_type, bool rotten_chunk);
static void  eat_chunk( int chunk_effect );
static void  eating(unsigned char item_class, int item_type);
static void  ghoul_eat_flesh( int chunk_effect );
static void  describe_food_change(int hunger_increment);
static bool  food_change(bool suppress_message);
bool vampire_consume_corpse(int mons_type, int mass,
                            int chunk_type, bool rotten);

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
        unwield_item(you.equip[EQ_WEAPON], false);

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
    // look for a butchering implement in your pack
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (is_valid_item( you.inv[i] )
            && can_cut_meat( you.inv[i] )
            && you.inv[i].base_type == OBJ_WEAPONS
            && item_known_uncursed(you.inv[i])
            && item_type_known(you.inv[i])
            && get_weapon_brand(you.inv[i]) != SPWPN_DISTORTION
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

bool butchery(void)
{
    bool wpn_switch = false;
    bool new_cursed = false;
    int old_weapon = you.equip[EQ_WEAPON];

    const transformation_type transform =
        static_cast<transformation_type>(you.attribute[ATTR_TRANSFORMATION]);
 
    // Xom probably likes this, occasionally
    bool teeth_butcher = (you.mutation[MUT_FANGS] == 3);
   
    bool barehand_butcher =
        (you.equip[ EQ_GLOVES ] == -1
         && (transform_can_butcher_barehanded(transform)
             || (transform == TRAN_NONE
                 && (you.species == SP_TROLL
                     || you.species == SP_GHOUL
                     || you.mutation[MUT_CLAWS]))));

    bool gloved_butcher = (you.species == SP_TROLL ||
                           you.species == SP_GHOUL ||
                           you.mutation[MUT_CLAWS]) &&
        (you.equip[EQ_GLOVES] != -1 &&
         !item_cursed(you.inv[you.equip[EQ_GLOVES]]));
    int old_gloves = you.equip[EQ_GLOVES];

    bool can_butcher = teeth_butcher || barehand_butcher ||
        (you.equip[EQ_WEAPON] != -1 &&
         can_cut_meat(you.inv[you.equip[EQ_WEAPON]]));
    
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

    if (player_is_levitating() && !wearing_amulet(AMU_CONTROLLED_FLIGHT))
    {
        mpr("You can't reach the floor from up here.");
        return (false);
    }

    // It makes more sense that you first find out if there's anything
    // to butcher, *then* decide to actually butcher it.
    // The old code did it the other way.
    if ( !can_butcher && you.duration[DUR_BERSERKER] )
    {
        mpr ("You are too berserk to search for a butchering knife!");
        return (false);
    }

    bool canceled_butcher = false;
    bool found_nonzero_corpses = false;
    for (int objl = igrd[you.x_pos][you.y_pos]; objl != NON_ITEM;
         objl = mitm[objl].link)
    {
        if ( (mitm[objl].base_type != OBJ_CORPSES) ||
             (mitm[objl].sub_type != CORPSE_BODY) )
            continue;

        found_nonzero_corpses = true;
        
        // offer the possibility of butchering
        snprintf(info, INFO_SIZE, "Butcher %s?",
                 mitm[objl].name(DESC_NOCAP_A).c_str());
        const int answer = yesnoquit( info, true, 'n', false );
        if ( answer == -1 )
        {
            canceled_butcher = true;
            break;
        }
        if ( answer == 0 )
            continue;

        bool removed_gloves = false;
        
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
            const int wpn = you.equip[EQ_WEAPON];
            if ( wpn_switch )
            {
                new_cursed =
                    (wpn != -1) &&
                    (you.inv[wpn].base_type == OBJ_WEAPONS) &&
                    item_cursed( you.inv[wpn]);
            }
            
            // note that if wpn == -1 the user selected '-' when
            // switching weapons
            
            if (!barehand_butcher &&
                (!wpn_switch || wpn == -1 || !can_cut_meat(you.inv[wpn])))
            {
                // still can't butcher. Early out
                if ( wpn == -1 ) {
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
            // we actually butcher now
            if ( teeth_butcher || barehand_butcher )
                mpr("You start tearing the corpse apart.");
            else
                mpr("You start hacking away.");

            bool rotten = (mitm[objl].special < 100);
            if (you.duration[DUR_PRAYER] && !rotten &&
                god_likes_butchery(you.religion))
            {
                offer_corpse(objl);
                destroy_item(objl);
            }
            else
            {
                // If we didn't switch weapons, we get in one turn of butchery;
                // otherwise the work has to happen in the delay.
                if (!wpn_switch && !removed_gloves)
                    ++mitm[objl].plus2;
                
                int work_req = 4 - mitm[objl].plus2;
                if (work_req < 0)
                    work_req = 0;

                start_delay(DELAY_BUTCHER, work_req, objl, mitm[objl].special);

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
        mprf("There isn't anything %sto dissect here.",
             found_nonzero_corpses? "else " : "");

    if (!new_cursed && wpn_switch) // should never happen
    {
        weapon_switch( old_weapon );
    }
    
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

bool prompt_eat_from_inventory(void)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return (false);
    }

    int which_inventory_slot = 
            prompt_invent_item(
                    "Eat which item?",
                    MT_INVLIST,
                    you.species == SP_VAMPIRE ? OBJ_CORPSES : OBJ_FOOD,
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
bool eat_food(bool run_hook)
{
    if (you.is_undead == US_UNDEAD)
    {
        mpr("You can't eat.");
        return (false);
    }

    if (you.hunger >= 11000)
    {
        mpr("You're too full to eat anything.");
        return (false);
    }

    // If user hook ran, we don't know whether something
    // was eaten or not...
    if (run_hook && userdef_eat_food())
        return (false);

    if (igrd[you.x_pos][you.y_pos] != NON_ITEM)
    {
        if (eat_from_floor())
        {
            burden_change();    // ghouls regain strength from rotten food
            return (true);
        }
    }

    return (prompt_eat_from_inventory());
}                               // end eat_food()

/*
 **************************************************
 *                                                *
 *              END PUBLIC FUNCTIONS              *
 *                                                *
 **************************************************
*/

static bool food_change(bool suppress_message)
{
    char newstate = HS_ENGORGED;
    bool state_changed = false;

    // this case shouldn't actually happen:
    if (you.is_undead == US_UNDEAD)
        you.hunger = 6000;

    // take care of ghouls - they can never be 'full'
    if (you.species == SP_GHOUL && you.hunger > 6999) 
        you.hunger = 6999;

    // vampires can never be engorged or starve to death
    if (you.species == SP_VAMPIRE && you.hunger <= 700)
        you.hunger = 701;
    if (you.species == SP_VAMPIRE && you.hunger > 10999)
        you.hunger = 10999;

    // get new hunger state
    if (you.hunger <= 1000)
        newstate = HS_STARVING;
    else if (you.hunger <= 2600)
        newstate = HS_HUNGRY;
    else if (you.hunger < 7000)
        newstate = HS_SATIATED;
    else if (you.hunger < 11000)
        newstate = HS_FULL;

    if (newstate != you.hunger_state)
    {
        state_changed = true;
        you.hunger_state = newstate;
        set_redraw_status( REDRAW_HUNGER );

        // Stop the travel command, if it's in progress and we just got hungry
        if (newstate < HS_SATIATED)
            interrupt_activity( AI_HUNGRY );

        if (suppress_message == false)
        {
            switch (you.hunger_state)
            {
            case HS_STARVING:
                mpr("You are starving!", MSGCH_FOOD);
                learned_something_new(TUT_YOU_STARVING);
                break;
            case HS_HUNGRY:
                mpr("You are feeling hungry.", MSGCH_FOOD);
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

    msg += ((you.hunger_state > HS_SATIATED) ? "full." : "hungry.");
    mpr(msg.c_str());
}                               // end describe_food_change()

void eat_from_inventory(int which_inventory_slot)
{
    if (you.inv[which_inventory_slot].base_type == OBJ_CORPSES
        && you.inv[which_inventory_slot].sub_type == CORPSE_BODY)
    {
        const int mons_type = you.inv[ which_inventory_slot ].plus;
        const int chunk_type = mons_corpse_effect( mons_type );
        const bool rotten = (you.inv[which_inventory_slot].special < 100);
        const int mass = item_mass( you.inv[which_inventory_slot] );

        if (!vampire_consume_corpse(mons_type, mass, chunk_type, rotten))
            return;

        if (!mons_skeleton( mons_type ) || one_chance_in(4))
        {
            dec_inv_item_quantity( which_inventory_slot, 1 );
        }
        else
        {
            you.inv[which_inventory_slot].sub_type = CORPSE_SKELETON;
            you.inv[which_inventory_slot].special = 90;
            you.inv[which_inventory_slot].colour = LIGHTGREY;
        }
        // dec_inv_item_quantity( which_inventory_slot, 1 );
        return;
    }
    else if (you.inv[which_inventory_slot].sub_type == FOOD_CHUNK)
    {
        // this is a bit easier to read... most compilers should
        // handle this the same -- bwr
        const int mons_type = you.inv[ which_inventory_slot ].plus;
        const int chunk_type = mons_corpse_effect( mons_type );
        const bool rotten = (you.inv[which_inventory_slot].special < 100);

        eat_chunk( determine_chunk_effect( chunk_type, rotten ) );
    }
    else
    {
        eating( you.inv[which_inventory_slot].base_type,
                you.inv[which_inventory_slot].sub_type );
    }

    dec_inv_item_quantity( which_inventory_slot, 1 );
}                               // end eat_from_inventory()

void eat_floor_item(int item_link)
{
    if (mitm[item_link].base_type == OBJ_CORPSES
        && mitm[item_link].sub_type == CORPSE_BODY)
    {
        const int mons_type = mitm[item_link].plus;
        const int chunk_type = mons_corpse_effect( mons_type );
        const bool rotten = (mitm[item_link].special < 100);
        const int mass = item_mass( mitm[item_link] );

        if (!vampire_consume_corpse(mons_type, mass, chunk_type, rotten))
            return;

        if (!mons_skeleton( mons_type ) || one_chance_in(4))
        {
            dec_mitm_item_quantity( item_link, 1 );
        }
        else
        {
            mitm[item_link].sub_type = CORPSE_SKELETON;
            mitm[item_link].special = 90;
            mitm[item_link].colour = LIGHTGREY;
        }
        // dec_mitm_item_quantity( item_link, 1 );

        you.turn_is_over = 1;
        return;
    }
    else if (mitm[item_link].sub_type == FOOD_CHUNK)
    {
        const int chunk_type = mons_corpse_effect( mitm[item_link].plus );
        const bool rotten = (mitm[item_link].special < 100);

        eat_chunk( determine_chunk_effect( chunk_type, rotten ) );
    }
    else
    {
        eating( mitm[item_link].base_type, mitm[item_link].sub_type );
    }

    you.turn_is_over = true;

    dec_mitm_item_quantity( item_link, 1 );
}

bool eat_from_floor(void)
{
    if (player_is_levitating() && !wearing_amulet(AMU_CONTROLLED_FLIGHT))
        return (false);

    bool need_more = false;
    for (int o = igrd[you.x_pos][you.y_pos]; o != NON_ITEM; o = mitm[o].link)
    {
        item_def& item = mitm[o];

        if (you.species != SP_VAMPIRE && item.base_type != OBJ_FOOD)
            continue;

        if (you.species == SP_VAMPIRE &&
            (item.base_type != OBJ_CORPSES || item.sub_type != CORPSE_BODY))
            continue;

        mprf( MSGCH_PROMPT,
              "%s %s%s?", you.species == SP_VAMPIRE ? "Drink blood from" : "Eat",
              (item.quantity > 1) ? "one of " : "",
              item.name(DESC_NOCAP_A).c_str() );

        // If we're prompting now, we don't need a -more- when
        // breaking out, because the prompt serves as a -more-. Of
        // course, the prompt can re-set need_more to true.
        need_more = false;

        unsigned char keyin = tolower( getch() );

        if (keyin == 0)
        {
            getch();
            keyin = 0;
        }

        if (keyin == 'q')
            return (false);

        if (keyin == 'y')
        {
            if (!can_ingest( item.base_type, item.sub_type, false ))
            {
                need_more = true;
                continue;
            }

            eat_floor_item(o);
            return (true);
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
        return (likes_chunks? nutrition
                : apply_herbivore_chunk_effects(nutrition));

    const int gourmand = 
        wearing_amulet(AMU_THE_GOURMAND)? 
            you.duration[DUR_GOURMAND]
          : 0;

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
static void eat_chunk( int chunk_effect )
{

    bool likes_chunks = (you.species == SP_OGRE || you.species == SP_TROLL ||
                         you.mutation[MUT_CARNIVOROUS] > 0);

    if (you.species == SP_GHOUL)
    {
        ghoul_eat_flesh( chunk_effect );
        start_delay( DELAY_EAT, 2 );
        lessen_hunger( CHUNK_BASE_NUTRITION, true );
    }
    else
    {
        switch (chunk_effect)
        {
        case CE_MUTAGEN_RANDOM:
            mpr("This meat tastes really weird.");
            mutate(RANDOM_MUTATION);
            xom_is_stimulated(100);
            break;

        case CE_MUTAGEN_BAD:
            mpr("This meat tastes *really* weird.");
            give_bad_mutation();
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
            mpr("There is something wrong with this meat.");
            if (disease_player( 50 + random2(100) ))
                xom_is_stimulated(random2(100));
            break;

        // note that this is the only case that takes time and forces redraw
        case CE_CLEAN:
        {
            say_chunk_flavour(likes_chunks);
            const int nutrition = chunk_nutrition(likes_chunks);
            start_delay( DELAY_EAT, 2, nutrition );
            lessen_hunger( nutrition, true );
            break;
        }
        }
    }

    return;
}                               // end eat_chunk()

static void ghoul_eat_flesh( int chunk_effect )
{
    bool healed = false;

    if (chunk_effect != CE_ROTTEN && chunk_effect != CE_CONTAMINATED)
    {
        mpr("This raw flesh tastes good.");

        if (!one_chance_in(5))
            healed = true;

        if (player_rotted() && !one_chance_in(3))
        {
            mpr("You feel more resilient.");
            unrot_hp(1);
        }
    }
    else
    {
        if (chunk_effect == CE_ROTTEN)
            mpr( "This rotting flesh tastes delicious!" );
        else // CE_CONTAMINATED
            mpr( "This flesh tastes delicious!" );

        healed = true;

        if (player_rotted() && !one_chance_in(4))
        {
            mpr("You feel more resilient.");
            unrot_hp(1);
        }
    }

    if (you.strength < you.max_strength && one_chance_in(5))
    {
        mpr("You feel your strength returning.");
        you.strength++;
        you.redraw_strength = 1;
    }

    if (healed && you.hp < you.hp_max)
        inc_hp(1 + random2(5) + random2(1 + you.experience_level), false);

    calc_hp();

    return;
}                               // end ghoul_eat_flesh()

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
        // for some reason, sausages do not penalize herbivores {dlb}:
        switch (item_type)
        {
        case FOOD_MEAT_RATION:
            carnivore_modifier = 500;
            herbivore_modifier = -1500;
            break;
        case FOOD_BEEF_JERKY:
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
        case FOOD_SAUSAGE:
            mpr("That sausage was delicious!");
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
                           || you.species == SP_OGRE
                           || you.species == SP_TROLL
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
    const int poison_resistance_level = player_res_poison();
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
        if (you.species == SP_GHOUL
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH
                || poison_resistance_level > 0 && you.species != SP_VAMPIRE)
        {
            this_chunk_effect = CE_CLEAN;
        }
        break;

    case CE_CONTAMINATED:
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH)
            this_chunk_effect = CE_CLEAN;
        else
        {
            switch (you.species)
            {
            case SP_GHOUL:
                // Doing this here causes a odd message later. -- bwr
                // this_chunk_effect = CE_ROTTEN;
                break;

            case SP_KOBOLD:
            case SP_TROLL:
                if (!one_chance_in(45))
                    this_chunk_effect = CE_CLEAN;
                break;

            case SP_HILL_ORC:
            case SP_OGRE:
                if (!one_chance_in(15))
                    this_chunk_effect = CE_CLEAN;
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
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH)
            this_chunk_effect = CE_CLEAN;
        else
        {
            switch (you.species)
            {
            case SP_KOBOLD:
            case SP_TROLL:
                if (!one_chance_in(15))
                    this_chunk_effect = CE_CLEAN;
                break;
            case SP_HILL_ORC:
            case SP_OGRE:
                if (!one_chance_in(5))
                    this_chunk_effect = CE_CLEAN;
                break;
            default:
                break;
            }
        }
    }

    // the amulet of the gourmad will permit consumption of
    // contaminated meat as though it were "clean" meat - ghouls get
    // rotting meat effect from clean chunks, since they love rotting
    // meat.
    if (wearing_amulet(AMU_THE_GOURMAND)
        && random2(GOURMAND_MAX) < you.duration[DUR_GOURMAND])
    {
        if (you.species == SP_GHOUL)
        {
            // [dshaligram] Ghouls relish contaminated meat.
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

bool vampire_consume_corpse(int mons_type, int mass,
                            int chunk_type, bool rotten)
{
    int food_value = 0;

    if (chunk_type == CE_HCL)
    {
        mpr( "There is no blood in this body!" );
        return false;
    }
    else if (!rotten)
    {
        inc_hp(1, false);

        switch (mons_type)
        {
           case MONS_HUMAN:
               food_value = mass + random2avg(you.experience_level * 10, 2);
               mpr( "This warm blood tastes really delicious!" );
               inc_hp(1 + random2(1 + you.experience_level), false);
               break;

           case MONS_ELF:
               food_value = mass + random2avg(you.experience_level * 10, 2);
               mpr( "This warm blood tastes magically delicious!" );
               inc_mp(1 + random2(3), false);
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
    else if (wearing_amulet(AMU_THE_GOURMAND))
    {
        food_value = mass/3 + random2(you.experience_level * 5);
        mpr("Slurps.");
        did_god_conduct(DID_DRINK_BLOOD, 8);
    }
    else
    {
        mpr("It's not fresh enough.");
        return false;
    }

    lessen_hunger( food_value, true );
    describe_food_change(food_value);

    if (player_rotted() && !rotten && one_chance_in(4))
    {
        mpr("You feel more resilient.");
        unrot_hp(1);
    }

    if (you.strength < you.max_strength && one_chance_in(3))
    {
        mpr("You feel your strength returning.");
        you.strength++;
        you.redraw_strength = 1;
    }

//    start_delay( DELAY_EAT, 3 );
    start_delay( DELAY_EAT, 1 + mass/300 );
    return true;
} // end vampire_consume_corpse()
