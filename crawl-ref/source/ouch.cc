/*
 *  File:       ouch.cc
 *  Summary:    Functions used when Bad Things happen to the player.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <8>      7/30/00        JDJ     Fixed end_game so that it works with filenames longer than 6 characters.
 *      <7>      19 June 2000   GDL     Changed handle to FILE *
 *      <6>      11/23/99       LRH     Fixed file purging for DOS?
 *      <5>      9/29/99        BCR     Fixed highscore so that it
 *                                      doesn't take so long.  Also
 *                                      added some whitespace to the scores.
 *                                      Fixed problem with uniques and 'a'.
 *      <4>      6/13/99        BWR     applied a mix of DML and my tmp
 *                                      file purging improvements.
 *      <3>      5/26/99        JDJ     highscore() will print more scores on
 *                                      larger windows.
 *      <2>      5/21/99        BWR     Added SCORE_FILE_ENTRIES, so
 *                                      that more top scores can be
 *                                      saved.
 *      <1>      -/--/--        LRH     Created
 */

#include "AppHdr.h"

#include <string.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#ifdef DOS
#include <conio.h>
#include <file.h>
#endif

#ifdef UNIX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "ouch.h"

#ifdef __MINGW32__
#include <io.h>
#endif

#include "externs.h"

#include "chardump.h"
#include "delay.h"
#include "files.h"
#include "hiscores.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "mon-util.h"
#include "notes.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "shopping.h"
#include "skills2.h"
#include "spells4.h"
#include "stuff.h"
#include "view.h"


void end_game( struct scorefile_entry &se );
void item_corrode( char itco );


/* NOTE: DOES NOT check for hellfire!!! */
int check_your_resists(int hurted, int flavour)
{
    int resist;

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "checking resistance: flavour=%d", flavour );
    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (flavour == BEAM_FIRE || flavour == BEAM_LAVA 
        || flavour == BEAM_HELLFIRE || flavour == BEAM_FRAG)
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        {
            mpr( "Your icy shield dissipates!", MSGCH_DURATION );
            you.duration[DUR_CONDENSATION_SHIELD] = 0;
            you.redraw_armour_class = 1;
        }
    }

    switch (flavour)
    {
    case BEAM_STEAM:
        resist = player_res_steam();
        if (resist > 0)
        {
            canned_msg(MSG_YOU_RESIST);
            hurted /= (1 + (resist * resist));
        }
        else if (resist < 0)
        {
            // We could use a superior message.
            mpr("It burns terribly!");
            hurted = hurted * 15 / 10;
        }
        break;

    case BEAM_FIRE:
        resist = player_res_fire();
        if (resist > 0)
        {
            canned_msg(MSG_YOU_RESIST);
            hurted /= (1 + (resist * resist));
        }
        else if (resist < 0)
        {
            mpr("It burns terribly!");
            hurted *= 15;
            hurted /= 10;
        }
        break;

    case BEAM_COLD:
        resist = player_res_cold();
        if (resist > 0)
        {
            canned_msg(MSG_YOU_RESIST);
            hurted /= (1 + (resist * resist));
        }
        else if (resist < 0)
        {
            mpr("You feel a terrible chill!");
            hurted *= 15;
            hurted /= 10;
        }
        break;

    case BEAM_ELECTRICITY:
        resist = player_res_electricity();
        if (resist >= 3)
        {
            canned_msg(MSG_YOU_RESIST);
            hurted = 0;
        }
        else if (resist > 0)
        {
            canned_msg(MSG_YOU_RESIST);
            hurted /= 3;
        }
        break;


    case BEAM_POISON:
        resist = player_res_poison();

        if (!resist)
            poison_player( coinflip() ? 2 : 1 );
        else
        {
            canned_msg(MSG_YOU_RESIST);
            hurted /= 3;
        }
        break;

    case BEAM_POISON_ARROW:
        // [dshaligram] NOT importing uber-poison arrow from 4.1. Giving no
        // bonus to poison resistant players seems strange and unnecessarily
        // arbitrary.
        resist = player_res_poison();

        if (!resist)
            poison_player( 6 + random2(3), true );
        else
        {
            mpr("You partially resist.");
            hurted /= 2;

            if (!you.is_undead)
                poison_player( 2 + random2(3), true ); 
        }
        break;

    case BEAM_NEG:
        resist = player_prot_life();

        if (resist > 0)
        {
            hurted -= (resist * hurted) / 3;
        }

        drain_exp();
        break;

    case BEAM_ICE:
        resist = player_res_cold();

        if (resist > 0)
        {
            mpr("You partially resist.");
            hurted /= 2;
        }
        else if (resist < 0)
        {
            mpr("You feel a painful chill!");
            hurted *= 13;
            hurted /= 10;
        }
        break;

    case BEAM_LAVA:
        resist = player_res_fire();

        if (resist > 1)
        {
            mpr("You partially resist.");
            hurted /= (1 + resist);
        }
        else if (resist < 0)
        {
            mpr("It burns terribly!");
            hurted *= 15;
            hurted /= 10;
        }
        break;

    case BEAM_ACID:
        if (player_res_acid())
        {
            canned_msg( MSG_YOU_RESIST );
            hurted = hurted * player_acid_resist_factor() / 100;
        }
        break;

    case BEAM_MIASMA:
        if (player_prot_life() > random2(3))
        {
            canned_msg( MSG_YOU_RESIST );
            hurted = 0;
        }
        break;

    case BEAM_HOLY:
        if (!you.is_undead && you.species != SP_DEMONSPAWN)
        {
            canned_msg( MSG_YOU_RESIST );
            hurted = 0;
        }
        break;
    }                           /* end switch */

    return (hurted);
}                               // end check_your_resists()

void splash_with_acid( char acid_strength )
{
    char splc = 0;
    int  dam = 0;

    const bool wearing_cloak = (you.equip[EQ_CLOAK] != -1);     

    for (splc = EQ_CLOAK; splc <= EQ_BODY_ARMOUR; splc++)
    {
        if (you.equip[splc] == -1)
        {
            if (!wearing_cloak || coinflip())
                dam += roll_dice( 1, acid_strength );

            continue;
        }

        if (random2(20) <= acid_strength)
            item_corrode( you.equip[splc] );
    }

    if (dam > 0)
    {
        const int post_res_dam = dam * player_acid_resist_factor() / 100;

        if (post_res_dam > 0)
        {
            mpr( "The acid burns!" );

            if (post_res_dam < dam)
                canned_msg(MSG_YOU_RESIST);

            ouch( post_res_dam, 0, KILLED_BY_ACID );
        }
    }
}                               // end splash_with_acid()

void weapon_acid( char acid_strength )
{
    char hand_thing = you.equip[EQ_WEAPON];

    if (hand_thing == -1)
        hand_thing = you.equip[EQ_GLOVES];

    if (hand_thing == -1)
    {
        snprintf( info, INFO_SIZE, "Your %s burn!", your_hand(true) );
        mpr( info );

        ouch( roll_dice( 1, acid_strength ), 0, KILLED_BY_ACID );  
    }
    else if (random2(20) <= acid_strength)
    {
        item_corrode( hand_thing );
    }
}                               // end weapon_acid()

void item_corrode( char itco )
{
    int chance_corr = 0;        // no idea what its full range is {dlb}
    bool it_resists = false;    // code simplifier {dlb}
    bool suppress_msg = false;  // code simplifier {dlb}
    int how_rusty = ((you.inv[itco].base_type == OBJ_WEAPONS) 
                                ? you.inv[itco].plus2 : you.inv[itco].plus);

    // early return for "oRC and cloak/preservation {dlb}:
    if (wearing_amulet(AMU_RESIST_CORROSION) && !one_chance_in(10))
    {
#if DEBUG_DIAGNOSTICS
        mpr( "Amulet protects.", MSGCH_DIAGNOSTICS );
#endif
        return;
    }

    // early return for items already pretty d*** rusty {dlb}:
    if (how_rusty < -5)
        return;

    // determine possibility of resistance by object type {dlb}:
    switch (you.inv[itco].base_type)
    {
    case OBJ_ARMOUR:
        if (is_random_artefact( you.inv[itco] ))
        {
            it_resists = true;
            suppress_msg = true;
        }
        else if ((you.inv[itco].sub_type == ARM_CRYSTAL_PLATE_MAIL
                    || get_equip_race(you.inv[itco]) == ISFLAG_DWARVEN)
                && !one_chance_in(5))
        {
            it_resists = true;
            suppress_msg = false;
        }
        break;

    case OBJ_WEAPONS:
        if (is_fixed_artefact(you.inv[itco]) 
            || is_random_artefact(you.inv[itco]))
        {
            it_resists = true;
            suppress_msg = true;
        }
        else if (get_equip_race(you.inv[itco]) == ISFLAG_DWARVEN
                && !one_chance_in(5))
        {
            it_resists = true;
            suppress_msg = false;
        }
        break;

    case OBJ_MISSILES:
        if (get_equip_race(you.inv[itco]) == ISFLAG_DWARVEN 
                && !one_chance_in(5))
        {
            it_resists = true;
            suppress_msg = false;
        }
        break;
    default:
	/* items which aren't missiles, etc...Could happen
	   if we're wielding a deck, say */
	return;
    }

    // determine chance of corrosion {dlb}:
    if (!it_resists)
    {
        chance_corr = abs( how_rusty );

        // ---------------------------- but it needs to stay this way
        //                              (as it *was* this way)

        // the embedded equation may look funny, but it actually works well
        // to generate a pretty probability ramp {6%, 18%, 34%, 58%, 98%}
        // for values [0,4] which closely matches the original, ugly switch
        // {dlb}
        if (chance_corr >= 0 && chance_corr <= 4)
        {
            it_resists = (random2(100) <
                            2 + (2 << (1 + chance_corr)) + (chance_corr * 8));
        }
        else
            it_resists = true;  // no idea how often this occurs {dlb}

        // if the checks get this far, you should hear about it {dlb}
        suppress_msg = false;
    }

    // handle message output and item damage {dlb}:
    if (!suppress_msg)
    {
        char str_pass[ ITEMNAME_SIZE ];
        in_name(itco, DESC_CAP_YOUR, str_pass);
        strcpy(info, str_pass);
        strcat(info, (it_resists) ? " resists." : " is eaten away!");
        mpr(info);
    }

    if (!it_resists)
    {
        how_rusty--;

        if (you.inv[itco].base_type == OBJ_WEAPONS)
            you.inv[itco].plus2 = how_rusty;
        else
            you.inv[itco].plus = how_rusty;

        you.redraw_armour_class = 1;     // for armour, rings, etc. {dlb}

        if (you.equip[EQ_WEAPON] == itco)
            you.wield_change = true;
    }

    return;
}                               // end item_corrode()

// Helper function for the expose functions below.
// This currently works because elements only target a single type each.
static int get_target_class( beam_type flavour )
{
    int target_class = OBJ_UNASSIGNED;

    switch (flavour)
    {
    case BEAM_FIRE:
    case BEAM_LAVA:
    case BEAM_NAPALM:
    case BEAM_HELLFIRE:
        target_class = OBJ_SCROLLS;
        break;

    case BEAM_COLD:
    case BEAM_FRAG:
        target_class = OBJ_POTIONS;
        break;

    case BEAM_SPORE:
        target_class = OBJ_FOOD;
        break;

    default:
        break;
    }

    return (target_class);
}

// XXX: These expose functions could use being reworked into a real system...
// the usage and implementation is currently very hacky.
// Handles the destruction of inventory items from the elements.
static void expose_invent_to_element( beam_type flavour, int strength )
{
    int i, j;
    int num_dest = 0;

    const int target_class = get_target_class( flavour );
    if (target_class == OBJ_UNASSIGNED)
        return;

    // Currently we test against each stack (and item in the stack)
    // independantly at strength%... perhaps we don't want that either
    // because it makes the system very fair and removes the protection
    // factor of junk (which might be more desirable for game play).
    for (i = 0; i < ENDOFPACK; i++)
    {
        if (!is_valid_item( you.inv[i] ))
            continue;

        if (is_valid_item( you.inv[i] )
            && (you.inv[i].base_type == target_class
                || (target_class == OBJ_FOOD 
                    && you.inv[i].base_type == OBJ_CORPSES)))
        {
            if (player_item_conserve() && !one_chance_in(10))
                continue;

            for (j = 0; j < you.inv[i].quantity; j++)
            {
                if (random2(100) < strength)
                {
                    num_dest++;

                    if (i == you.equip[EQ_WEAPON])
                        you.wield_change = true;

                    if (dec_inv_item_quantity( i, 1 ))
                        break;
                }
            }
        }
    }

    if (num_dest > 0)
    {
        switch (target_class)
        {
        case OBJ_SCROLLS:
            snprintf( info, INFO_SIZE, "%s you are carrying %s fire!",
                      (num_dest > 1) ? "Some of the scrolls" : "A scroll",
                      (num_dest > 1) ? "catch" : "catches" );
            break;

        case OBJ_POTIONS:
            snprintf( info, INFO_SIZE, "%s you are carrying %s and %s!",
                      (num_dest > 1) ? "Some of the potions" : "A potion",
                      (num_dest > 1) ? "freeze" : "freezes",
                      (num_dest > 1) ? "shatter" : "shatters" );
            break;

        case OBJ_FOOD:
            snprintf( info, INFO_SIZE, "Some of your food is covered with spores!" );
            break;

        default:
            snprintf( info, INFO_SIZE, "%s you are carrying %s destroyed!",
                      (num_dest > 1) ? "Some items" : "An item",
                      (num_dest > 1) ? "were" : "was" );
            break;
        }

        mpr( info );    // XXX: should this be in a channel?
    }
}


// Handle side-effects for exposure to element other than damage.
// This function exists because some code calculates its own damage 
// instead of using check_resists and we want to isolate all the special
// code they keep having to do... namely condensation shield checks, 
// you really can't expect this function to even be called for much else.
//
// This function now calls expose_invent_to_element if strength > 0.
//
// XXX: this function is far from perfect and a work in progress.
void expose_player_to_element( beam_type flavour, int strength )
{ 
    // Note that BEAM_TELEPORT is sent here when the player 
    // blinks or teleports.
    if (flavour == BEAM_FIRE || flavour == BEAM_LAVA 
        || flavour == BEAM_HELLFIRE || flavour == BEAM_FRAG 
        || flavour == BEAM_TELEPORT || flavour == BEAM_NAPALM 
        || flavour == BEAM_STEAM)
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        {
            mprf(MSGCH_DURATION, "Your icy shield dissipates!");
            you.duration[DUR_CONDENSATION_SHIELD] = 0;
            you.redraw_armour_class = true;
        }
    }

    if (strength)
        expose_invent_to_element( flavour, strength );
}

void lose_level(void)
{
    // because you.experience is unsigned long, if it's going to be -ve
    // must die straightaway.
    if (you.experience_level == 1)
        ouch( -9999, 0, KILLED_BY_DRAINING );

    you.experience = exp_needed( you.experience_level + 1 ) - 1;
    you.experience_level--;

    snprintf( info, INFO_SIZE, "You are now a level %d %s!", 
             you.experience_level, you.class_name );
    mpr( info, MSGCH_WARN );

    // Constant value to avoid grape jelly trick... see level_change() for
    // where these HPs and MPs are given back.  -- bwr
    ouch( 4, 0, KILLED_BY_DRAINING );
    dec_max_hp(4);

    dec_mp(1);
    dec_max_mp(1);

    calc_hp();
    calc_mp();

    char buf[200];
    sprintf(buf, "HP: %d/%d MP: %d/%d",
	    you.hp, you.hp_max, you.magic_points, you.max_magic_points);
    take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0, buf));

    you.redraw_experience = 1;
}                               // end lose_level()

void drain_exp(void)
{
    int protection = player_prot_life();

    if (you.duration[DUR_PRAYER]
        && (you.religion == GOD_ZIN || you.religion == GOD_SHINING_ONE)
        && random2(150) < you.piety)
    {
        simple_god_message(" protects your life force!");
        return;
    }

    if (protection >= 3 || you.is_undead)
    {
        mpr("You fully resist.");
        return;
    }

    if (you.experience == 0)
        ouch(-9999, 0, KILLED_BY_DRAINING);

    if (you.experience_level == 1)
    {
        you.experience = 0;
        return;
    }

    unsigned long total_exp = exp_needed( you.experience_level + 2 )
                                    - exp_needed( you.experience_level + 1 );
    unsigned long exp_drained = total_exp * (10 + random2(11));

    exp_drained /= 100;

    if (protection > 0)
    {
        mpr("You partially resist.");
        exp_drained -= (protection * exp_drained) / 3;
    }

    if (exp_drained > 0)
    {
        mpr("You feel drained.");
        you.experience -= exp_drained;
        you.exp_available -= exp_drained;

        if (you.exp_available < 0)
            you.exp_available = 0;

#if DEBUG_DIAGNOSTICS
        snprintf( info, INFO_SIZE, "You lose %ld experience points.",
                  exp_drained );
        mpr( info, MSGCH_DIAGNOSTICS );
#endif

        you.redraw_experience = 1;

        if (you.experience < exp_needed(you.experience_level + 1))
            lose_level();
    }
}                               // end drain_exp()

// death_source should be set to zero for non-monsters {dlb}
void ouch( int dam, int death_source, char death_type, const char *aux )
{
    ait_hp_loss hpl(dam, death_type);
    interrupt_activity( AI_HP_LOSS, &hpl );

    if (you.deaths_door && death_type != KILLED_BY_LAVA
                                    && death_type != KILLED_BY_WATER)
    {
        return;
    }

    // assumed bug for high damage amounts
    if (dam > 300)
    {
        snprintf( info, INFO_SIZE, 
                  "Potential bug: Unexpectedly high damage = %d", dam );
        mpr( info, MSGCH_DANGER );
        return;                 
    }

    if (dam > -9000)            // that is, a "death" caused by hp loss {dlb}
    {
        switch (you.religion)
        {
        case GOD_XOM:
            if (random2(you.hp_max) > you.hp && dam > random2(you.hp)
                                                    && one_chance_in(5))
            {
                simple_god_message( " protects you from harm!" );
                return;
            }
            break;

        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
        case GOD_YREDELEMNUL:
            if (dam >= you.hp && you.duration[DUR_PRAYER]
                                                && random2(you.piety) >= 30)
            {
                simple_god_message( " protects you from harm!" );
                return;
            }
            break;
        }

        // Damage applied here:
        dec_hp( dam, true );

        // Even if we have low HP messages off, we'll still give a
        // big hit warning (in this case, a hit for half our HPs) -- bwr
        if (dam > 0 && you.hp_max <= dam * 2)
            mpr( "Ouch!  That really hurt!", MSGCH_DANGER );

        if (you.hp > 0 && Options.hp_warning
            && you.hp <= (you.hp_max * Options.hp_warning) / 100)
        {
            mpr( "* * * LOW HITPOINT WARNING * * *", MSGCH_DANGER );
        }
	take_note(
                Note(
                    NOTE_HP_CHANGE, 
                    you.hp, 
                    you.hp_max, 
                    scorefile_entry(dam, death_source, death_type, aux, true)
                        .death_description(scorefile_entry::DDV_TERSE)
                        .c_str()) );

        if (you.hp > 0)
            return;
    }

#ifdef WIZARD
    if (death_type != KILLED_BY_QUITTING 
        && death_type != KILLED_BY_WINNING
        && death_type != KILLED_BY_LEAVING)
    {
        if (you.wizard)
        {
#ifdef USE_OPTIONAL_WIZARD_DEATH

#if DEBUG_DIAGNOSTICS
            snprintf( info, INFO_SIZE, "Damage: %d; Hit points: %d", dam, you.hp );
            mpr( info, MSGCH_DIAGNOSTICS );
#endif // DEBUG_DIAGNOSTICS

            if (!yesno("Die?", false, 'n'))
            {
                set_hp(you.hp_max, false);
                return;
            }
#else  // !def USE_OPTIONAL_WIZARD_DEATH
            mpr("Since you're a debugger, I'll let you live.");
            mpr("Be more careful next time, okay?");

            set_hp(you.hp_max, false);
            return;
#endif  // USE_OPTIONAL_WIZARD_DEATH
        }
    }
#endif  // WIZARD

    //okay, so you're dead:

    // prevent bogus notes
    activate_notes(false);

    // CONSTRUCT SCOREFILE ENTRY
    scorefile_entry se(dam, death_source, death_type, aux);

#ifdef SCORE_WIZARD_CHARACTERS
    // add this highscore to the score file.
    hiscores_new_entry(se);
#else

    // only add non-wizards to the score file.
    // never generate bones files of wizard characters -- bwr
    if (!you.wizard)
    {
        hiscores_new_entry(se);

        if (death_type != KILLED_BY_LEAVING && death_type != KILLED_BY_WINNING)
            save_ghost();
    }

#endif

    end_game(se);
}

void end_game( struct scorefile_entry &se )
{
    int i;
    bool dead = true;

    if (se.death_type == KILLED_BY_LEAVING ||
        se.death_type == KILLED_BY_WINNING)
    {
        dead = false;
    }

    // clean all levels that we think we have ever visited
    for (int level = 0; level < MAX_LEVELS; level++)
    {
        for (int dungeon = 0; dungeon < MAX_BRANCHES; dungeon++)
        {
            if (tmp_file_pairs[level][dungeon])
            {
                unlink(make_filename( you.your_name, level, dungeon,
                                      false, false ).c_str());
            }
        }
    }

    // temp level, if any
    unlink( make_filename( you.your_name, 0, 0, true, false ).c_str() );

    // create base file name
    std::string basename = get_savedir_filename( you.your_name, "", "" );

    const char* suffixes[] = {
#ifdef CLUA_BINDINGS
	".lua",
#endif
#ifdef PACKAGE_SUFFIX
	PACKAGE_SUFFIX ,
#endif
	".st", ".kil", ".tc", ".nts", ".sav"
    };

    const int num_suffixes = sizeof(suffixes) / sizeof(const char*);

    for ( i = 0; i < num_suffixes; ++i ) {
	std::string tmpname = basename + suffixes[i];
	unlink( tmpname.c_str() );
    }

    // death message
    if (dead)
    {
        mpr("You die...");      // insert player name here? {dlb}
        viewwindow(1, false);   // don't do this for leaving/winning characters
    }
    more();

    for (i = 0; i < ENDOFPACK; i++)
        set_ident_flags( you.inv[i], ISFLAG_IDENT_MASK );

    for (i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].base_type != 0)
        {
            set_ident_type( you.inv[i].base_type, 
                            you.inv[i].sub_type, ID_KNOWN_TYPE );
        }
    }

    invent( -1, dead );
    clrscr();

    if (!dump_char( "morgue", !dead, true ))
        mpr("Char dump unsuccessful! Sorry about that.");
#if DEBUG_DIAGNOSTICS
    //jmf: switched logic and moved "success" message to debug-only
    else
        mpr("Char dump successful! (morgue.txt).");
#endif // DEBUG

    more();

    clrscr();
#ifdef DOS_TERM
    window(1, 1, 80, 25);
#endif

    clrscr();
    cprintf( "Goodbye, %s.", you.your_name );
    cprintf( EOL EOL "    " ); // Space padding where # would go in list format

    char scorebuff[ HIGHSCORE_SIZE ];

    hiscores_format_single_long( scorebuff, se, true );
    // truncate
    scorebuff[ HIGHSCORE_SIZE - 1 ] = 0;

    const int lines = count_occurrences(scorebuff, EOL) + 1;

    cprintf( scorebuff );

    cprintf( EOL "Best Crawlers -" EOL );

    // "- 5" gives us an extra line in case the description wraps on a line.
    hiscores_print_list( get_number_of_lines() - lines - 5 );

    // just to pause, actual value returned does not matter {dlb}
    get_ch();
    end(0);
}
