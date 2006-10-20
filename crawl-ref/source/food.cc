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

static int   determine_chunk_effect(int which_chunk_type, bool rotten_chunk);
static void  eat_chunk( int chunk_effect );
static void  eating(unsigned char item_class, int item_type);
static void  ghoul_eat_flesh( int chunk_effect );
static void  describe_food_change(int hunger_increment);
static bool  food_change(bool suppress_message);

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
        char buff[80];
        in_name( targ, DESC_NOCAP_A, buff );

        char let = index_to_letter( targ );

        snprintf( info, INFO_SIZE, "Switching back to %c - %s.", let, buff );
        mpr( info );
    }

    // unwield the old weapon and wield the new.
    // XXX This is a pretty dangerous hack;  I don't like it.--GDL
    //
    // Well yeah, but that's because interacting with the wielding
    // code is a mess... this whole function's purpose was to 
    // isolate this hack until there's a proper way to do things. -- bwr
    if (you.equip[EQ_WEAPON] != -1)
        unwield_item(you.equip[EQ_WEAPON]);

    you.equip[EQ_WEAPON] = targ;

    // special checks: staves of power, etc
    if (targ != -1)
        wield_effects( targ, false );
}

// look for a butchering implement, prompting user if no obvious
// options exist. Returns whether a weapon was switched.
static bool find_butchering_implement() {

    // look for a butchering implement in your pack
    for (int i = 0; i < ENDOFPACK; ++i) {
	if (is_valid_item( you.inv[i] )
	    && can_cut_meat( you.inv[i] )
	    && you.inv[i].base_type == OBJ_WEAPONS
	    && item_known_uncursed(you.inv[i])
	    && item_ident( you.inv[i], ISFLAG_KNOW_TYPE )
	    && get_weapon_brand(you.inv[i]) != SPWPN_DISTORTION
	    && can_wield( &you.inv[i] ))
	{
	    mpr("Switching to a butchering implement.");
	    wield_weapon( true, i, false );

	    // Account for the weapon switch...we're doing this here
	    // since the above switch may reveal information about the
	    // weapon (curse status, ego type).  So even if the
	    // character fails to or decides not to butcher past this
	    // point, they have achieved something and there should be
	    // a cost.
	    start_delay( DELAY_UNINTERRUPTIBLE, 1 );
	    return true;
	}
    }
    
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
    char str_pass[ ITEMNAME_SIZE ];

    bool can_butcher = false;
    bool wpn_switch = false;
    bool new_cursed = false;
    int old_weapon = you.equip[EQ_WEAPON];

    bool barehand_butcher = (you.equip[ EQ_GLOVES ] == -1)
      && (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS ||
	  you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON ||
	  (you.attribute[ATTR_TRANSFORMATION] == TRAN_NONE &&
	   (you.species == SP_TROLL ||
	    you.species == SP_GHOUL ||
	    you.mutation[MUT_CLAWS])));
    
    if (igrd[you.x_pos][you.y_pos] == NON_ITEM)
    {
        mpr("There isn't anything here!");
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
    if ( !barehand_butcher && you.berserker ) {
	mpr ("You are too berserk to search for a butchering knife!");
	return (false);
    }

    int objl;

    for (objl = igrd[you.x_pos][you.y_pos]; objl != NON_ITEM;
	 objl = mitm[objl].link) {

	if ( (mitm[objl].base_type != OBJ_CORPSES) ||
	     (mitm[objl].sub_type != CORPSE_BODY) )
	    continue;

	// offer the possibility of butchering
	it_name(objl, DESC_NOCAP_A, str_pass);
	snprintf(info, INFO_SIZE, "Butcher %s?", str_pass);
	int answer = yesnoquit( info, true, 0, false );
	if ( answer == -1 )
	    break;
	if ( answer == 0 )
	    continue;
	
	can_butcher = barehand_butcher ||
	    (you.equip[EQ_WEAPON] != -1 &&
	     can_cut_meat(you.inv[you.equip[EQ_WEAPON]]));
	
	if ( Options.easy_butcher && !can_butcher ) {

	    // try to find a butchering implement
	    wpn_switch = find_butchering_implement();
	    const int wpn = you.equip[EQ_WEAPON];
	    if ( wpn_switch ) {
		new_cursed =
		    (wpn != -1) &&
		    (you.inv[wpn].base_type == OBJ_WEAPONS) &&
		    item_cursed( you.inv[wpn]);
	    }
	    
	    // note that barehanded butchery would not reach this
	    // stage, so if wpn == -1 the user selected '-' when
	    // switching weapons
	    
	    if (!wpn_switch || wpn == -1 || !can_cut_meat(you.inv[wpn])) {
		
		// still can't butcher. Early out
		if ( wpn == -1 ) {
		    if (you.equip[EQ_GLOVES] == -1)
			mpr("What, with your bare hands?");
		    else
			mpr("You can't use your claws with your gloves on!");
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

	if ( can_butcher ) {

	    // we actually butcher now
	    if ( barehand_butcher )
		mpr("You start tearing the corpse apart.");
	    else
		mpr("You start hacking away.");

	    if (you.duration[DUR_PRAYER] &&
		(you.religion == GOD_OKAWARU || you.religion == GOD_MAKHLEB ||
		 you.religion == GOD_TROG)) {
		offer_corpse(objl);
		destroy_item(objl);
	    }
	    else {
		int work_req = 3 - mitm[objl].plus2;
		if (work_req < 0)
		    work_req = 0;

		start_delay(DELAY_BUTCHER, work_req, objl, mitm[objl].special);
	    }
        }

        // switch weapon back
        if (!new_cursed && wpn_switch)
            start_delay( DELAY_WEAPON_SWAP, 1, old_weapon );

        you.turn_is_over = true;

        return true;
    }

    mpr("There isn't anything to dissect here.");

    if (!new_cursed && wpn_switch) { // should never happen
        weapon_switch( old_weapon );
    }
    
    return false;
}                               // end butchery()

#ifdef CLUA_BINDINGS
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
#endif

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
                    MT_INVSELECT,
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
    if (you.inv[which_inventory_slot].base_type != OBJ_FOOD)
    {
        mpr("You can't eat that!");
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
                break;
            case HS_HUNGRY:
                mpr("You are feeling hungry.", MSGCH_FOOD);
                break;
            default:
                break;
            }
        }
    }

    return (state_changed);
}                               // end food_change()


// food_increment is positive for eating,  negative for hungering
static void describe_food_change(int food_increment)
{
    int magnitude = (food_increment > 0)?food_increment:(-food_increment);

    if (magnitude == 0)
        return;

    strcpy(info, (magnitude <= 100) ? "You feel slightly " :
                 (magnitude <= 350) ? "You feel somewhat " :
                 (magnitude <= 800) ? "You feel a quite a bit "
                                    : "You feel a lot ");

    if ((you.hunger_state > HS_SATIATED) ^ (food_increment < 0))
        strcat(info, "more ");
    else
        strcat(info, "less ");

    strcat(info, (you.hunger_state > HS_SATIATED) ? "full."
                                                  : "hungry.");
    mpr(info);
}                               // end describe_food_change()

void eat_from_inventory(int which_inventory_slot)
{
    if (you.inv[which_inventory_slot].sub_type == FOOD_CHUNK)
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
    if (mitm[item_link].sub_type == FOOD_CHUNK)
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
    char str_pass[ ITEMNAME_SIZE ];

    if (player_is_levitating() && !wearing_amulet(AMU_CONTROLLED_FLIGHT))
        return (false);

    for (int o = igrd[you.x_pos][you.y_pos]; o != NON_ITEM; o = mitm[o].link)
    {
        if (mitm[o].base_type != OBJ_FOOD)
            continue;

        it_name( o, DESC_NOCAP_A, str_pass );
        snprintf( info, INFO_SIZE, "Eat %s%s?", (mitm[o].quantity > 1) ? "one of " : "", 
                 str_pass );
        mpr( info, MSGCH_PROMPT );

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
            if (!can_ingest( mitm[o].base_type, mitm[o].sub_type, false ))
                return (false);

            eat_floor_item(o);
            return (true);
        }
    }

    return (false);
}


// never called directly - chunk_effect values must pass
// through food::determine_chunk_effect() first {dlb}:
static void eat_chunk( int chunk_effect )
{

    bool likes_chunks = (you.species == SP_KOBOLD || you.species == SP_OGRE
                         || you.species == SP_TROLL
                         || you.mutation[MUT_CARNIVOROUS] > 0);

    if (you.species == SP_GHOUL)
    {
        ghoul_eat_flesh( chunk_effect );
        start_delay( DELAY_EAT, 2 );
        lessen_hunger( 1000, true );
    }
    else
    {
        switch (chunk_effect)
        {
        case CE_MUTAGEN_RANDOM:
            mpr("This meat tastes really weird.");
            mutate(100);
            break;

        case CE_MUTAGEN_BAD:
            mpr("This meat tastes *really* weird.");
            give_bad_mutation();
            break;

        case CE_HCL:
            rot_player( 10 + random2(10) );
            disease_player( 50 + random2(100) );
            break;

        case CE_POISONOUS:
            mpr("Yeeuch - this meat is poisonous!");
            poison_player( 3 + random2(4) );
            break;

        case CE_ROTTEN:
        case CE_CONTAMINATED:
            mpr("There is something wrong with this meat.");
            disease_player( 50 + random2(100) );
            break;

        // note that this is the only case that takes time and forces redraw
        case CE_CLEAN:
            strcpy(info, "This raw flesh ");

            strcat(info, (likes_chunks) ? "tastes good."
                                        : "is not very appetising.");
            mpr(info);

            start_delay( DELAY_EAT, 2 );
            lessen_hunger( 1000, true );
            break;
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
                strcpy(info, "That beef jerky was ");
                strcat(info, (one_chance_in(4)) ? "jerk-a-riffic"
                                                : "delicious");
                strcat(info, "!");
                mpr(info);
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
                strcpy(info, "Mmmm... Yummy ");
                strcat(info, (item_type == FOOD_APPLE)   ? "apple." :
                             (item_type == FOOD_PEAR)    ? "pear." :
                             (item_type == FOOD_APRICOT) ? "apricot."
                                                         : "fruit.");
                mpr(info);
                break;
            case FOOD_CHOKO:
                mpr("That choko was very bland.");
                break;
            case FOOD_SNOZZCUMBER:
                mpr("That snozzcumber tasted truly putrid!");
                break;
            case FOOD_ORANGE:
                strcpy(info, "That orange was delicious!");
                if (one_chance_in(8))
                    strcat(info, " Even the peel tasted good!");
                mpr(info);
                break;
            case FOOD_BANANA:
                strcpy(info, "That banana was delicious!");
                if (one_chance_in(8))
                    strcat(info, " Even the peel tasted good!");
                mpr(info);
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
            if (SysEnv.crawl_pizza && !one_chance_in(3))
                snprintf(info, INFO_SIZE, "Mmm... %s", SysEnv.crawl_pizza);
            else
            {
                temp_rand = random2(9);

                snprintf(info, INFO_SIZE, "Mmm... %s",
                             (temp_rand == 0) ? "Ham and pineapple." :
                             (temp_rand == 2) ? "Vegetable." :
                             (temp_rand == 3) ? "Pepperoni." :
                             (temp_rand == 4) ? "Yeuchh - Anchovies!" :
                             (temp_rand == 5) ? "Cheesy." :
                             (temp_rand == 6) ? "Supreme." :
                             (temp_rand == 7) ? "Super Supreme!"
                                              : "Chicken.");
            }
            mpr(info);
            break;
        case FOOD_CHEESE:
            strcpy(info, "Mmm... ");
            temp_rand = random2(9);

            strcat(info, (temp_rand == 0) ? "Cheddar" :
                         (temp_rand == 1) ? "Edam" :
                         (temp_rand == 2) ? "Wensleydale" :
                         (temp_rand == 3) ? "Camembert" :
                         (temp_rand == 4) ? "Goat cheese" :
                         (temp_rand == 5) ? "Fruit cheese" :
                         (temp_rand == 6) ? "Mozzarella" :
                         (temp_rand == 7) ? "Sheep cheese"
                                          : "Yak cheese");

            strcat(info, ".");
            mpr(info);
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


    bool ur_carnivorous = (you.species == SP_GHOUL
                           || you.species == SP_KOBOLD
                           || you.mutation[MUT_CARNIVOROUS] == 3);

    bool ur_herbivorous = (you.mutation[MUT_HERBIVOROUS] > 1);

    // ur_chunkslover not defined in terms of ur_carnivorous because
    // a player could be one and not the other IMHO - 13mar2000 {dlb}
    bool ur_chunkslover = ( 
                (check_hunger? you.hunger_state <= HS_HUNGRY : true)
                           || wearing_amulet(AMU_THE_GOURMAND, !reqid)
                           || you.species == SP_KOBOLD
                           || you.species == SP_OGRE
                           || you.species == SP_TROLL
                           || you.species == SP_GHOUL
                           || you.mutation[MUT_CARNIVOROUS]);

    switch (what_isit)
    {
    case OBJ_FOOD:
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

    // other object types are set to return false for now until
    // someone wants to recode the eating code to permit consumption
    // of things other than just food -- corpses first, then more
    // exotic stuff later would be good to explore - 13mar2000 {dlb}
    case OBJ_CORPSES:
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
    int poison_resistance_level = player_res_poison();
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
                || poison_resistance_level > 0)
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

    // the amulet of the gourmad will permit consumption of rotting meat as
    // though it were "clean" meat - ghouls can expect the reverse, as they
    // prize rotten meat ... yum! {dlb}:
    if (wearing_amulet(AMU_THE_GOURMAND))
    {
        if (you.species == SP_GHOUL)
        {
            if (this_chunk_effect == CE_CLEAN)
                this_chunk_effect = CE_ROTTEN;
        }
        else
        {
            if (this_chunk_effect == CE_ROTTEN)
                this_chunk_effect = CE_CLEAN;
        }
    }

    return (this_chunk_effect);
}                               // end determine_chunk_effect()
