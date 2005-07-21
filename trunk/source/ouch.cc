/*
 *  File:       ouch.cc
 *  Summary:    Functions used when Bad Things happen to the player.
 *  Written by: Linley Henzell
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
#include "ouch.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#ifdef DOS
#include <conio.h>
#include <file.h>
#endif

#ifdef LINUX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef USE_EMX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef OS9
#include <stat.h>
#else
#include <sys/stat.h>
#endif

#include "externs.h"

#include "chardump.h"
#include "delay.h"
#include "files.h"
#include "hiscores.h"
#include "invent.h"
#include "itemname.h"
#include "items.h"
#include "macro.h"
#include "mon-util.h"
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
        || flavour == BEAM_HELLFIRE || flavour == BEAM_EXPLOSION
        || flavour == BEAM_FRAG)
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
        resist = player_res_poison();

        if (!resist)
            poison_player( 6 + random2(3), true );
        else
        {
            mpr("You partially resist.");
            hurted /= 2;

            if (!you.is_undead)
                poison_player( coinflip() ? 2 : 3, true ); 
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
    }                           /* end switch */

    return (hurted);
}                               // end check_your_resists()

void splash_with_acid( char acid_strength )
{
    /* affects equip only?
       assume that a message has already been sent, also that damage has
       already been done
     */
    char splc = 0;
    int  dam = 0;

    const bool wearing_cloak = (you.equip[EQ_CLOAK] == -1);     

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

    if (dam)
    {
        mpr( "The acid burns!" );
        ouch( dam, 0, KILLED_BY_ACID );
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
                    || cmp_equip_race( you.inv[itco], ISFLAG_DWARVEN ))
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
        else if (cmp_equip_race( you.inv[itco], ISFLAG_DWARVEN ) 
                && !one_chance_in(5))
        {
            it_resists = true;
            suppress_msg = false;
        }
        break;

    case OBJ_MISSILES:
        if (cmp_equip_race( you.inv[itco], ISFLAG_DWARVEN ) && !one_chance_in(5))
        {
            it_resists = true;
            suppress_msg = false;
        }
        break;
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

void scrolls_burn(char burn_strength, char target_class)
{

    unsigned char burnc;
    unsigned char burn2;
    unsigned char burn_no = 0;

    if (wearing_amulet(AMU_CONSERVATION) && !one_chance_in(10))
    {
#if DEBUG_DIAGNOSTICS
        mpr( "Amulet conserves.", MSGCH_DIAGNOSTICS );
#endif
        return;
    }

    for (burnc = 0; burnc < ENDOFPACK; burnc++)
    {
        if (!you.inv[burnc].quantity)
            continue;
        if (you.inv[burnc].base_type != target_class)
            continue;

        for (burn2 = 0; burn2 < you.inv[burnc].quantity; burn2++)
        {
            if (random2(70) < burn_strength)
            {
                burn_no++;

                if (burnc == you.equip[EQ_WEAPON])
                    you.wield_change = true;

                if (dec_inv_item_quantity( burnc, 1 ))
                    break;
            }
        }
    }

    if (burn_no == 1)
    {
        if (target_class == OBJ_SCROLLS)
            mpr("A scroll you are carrying catches fire!");
        else if (target_class == OBJ_POTIONS)
            mpr("A potion you are carrying freezes and shatters!");
        else if (target_class == OBJ_FOOD)
            mpr("Some of your food is covered with spores!");
    }
    else if (burn_no > 1)
    {
        if (target_class == OBJ_SCROLLS)
            mpr("Some of the scrolls you are carrying catch fire!");
        else if (target_class == OBJ_POTIONS)
            mpr("Some of the potions you are carrying freeze and shatter!");
        else if (target_class == OBJ_FOOD)
            mpr("Some of your food is covered with spores!");
    }
    /* burn_no could be 0 */
}

                            // end scrolls_burn()
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
    int d = 0;
    int e = 0;

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

    if (you_are_delayed())
    {
        stop_delay();
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
        case GOD_OKAWARU:
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

            if (!yesno("Die?", false))
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

    // do points first.
    long points = you.gold;
    points += (you.experience * 7) / 10;

    //if (death_type == KILLED_BY_WINNING) points += points / 2;
    //if (death_type == KILLED_BY_LEAVING) points += points / 10;
    // these now handled by giving player the value of their inventory
    char temp_id[4][50];

    for (d = 0; d < 4; d++)
    {
        for (e = 0; e < 50; e++)
            temp_id[d][e] = 1;
    }

    // CONSTRUCT SCOREFILE ENTRY
    struct scorefile_entry se;

    // Score file entry version:
    //
    // 4.0      - original versioned entry
    // 4.1      - added real_time and num_turn fields
    // 4.2      - stats and god info

    se.version = 4;
    se.release = 2;

    strncpy( se.name, you.your_name, kNameLen );
    se.name[ kNameLen - 1 ] = '\0';
#ifdef MULTIUSER
    se.uid = (int) getuid();
#else
    se.uid = 0;
#endif

    FixedVector< int, NUM_RUNE_TYPES >  rune_array;

    se.num_runes = 0;
    se.num_diff_runes = 0;

    for (int i = 0; i < NUM_RUNE_TYPES; i++)
        rune_array[i] = 0;

    // Calculate value of pack and runes when character leaves dungeon
    if (death_type == KILLED_BY_LEAVING || death_type == KILLED_BY_WINNING)
    {
        for (d = 0; d < ENDOFPACK; d++)
        {
            if (is_valid_item( you.inv[d] ))
            {
                points += item_value( you.inv[d], temp_id, true );

                if (you.inv[d].base_type == OBJ_MISCELLANY
                    && you.inv[d].sub_type == MISC_RUNE_OF_ZOT)
                {
                    if (rune_array[ you.inv[d].plus ] == 0)
                        se.num_diff_runes++;

                    se.num_runes += you.inv[d].quantity;
                    rune_array[ you.inv[d].plus ] += you.inv[d].quantity;
                }
            }
        }

        // Bonus for exploring different areas, not for collecting a 
        // huge stack of demonic runes in Pandemonium (gold value 
        // is enough for those). -- bwr
        if (se.num_diff_runes >= 3)
            points += ((se.num_diff_runes + 2) * (se.num_diff_runes + 2) * 1000);
    }

    // Players will have a hard time getting 1/10 of this (see XP cap):
    if (points > 99999999)
        points = 99999999;

    se.points = points;
    se.race = you.species;
    se.cls = you.char_class;

    // strcpy(se.race_class_name, "");
    se.race_class_name[0] = '\0';

    se.lvl = you.experience_level;
    se.best_skill = best_skill( SK_FIGHTING, NUM_SKILLS - 1, 99 );
    se.best_skill_lvl = you.skills[ se.best_skill ];
    se.death_type = death_type;

    // for death by monster

    // Set the default aux data value... 
    // If aux is passed in (ie for a trap), we'll default to that.
    if (aux == NULL)
        se.auxkilldata[0] = '\0';
    else
    {
        strncpy( se.auxkilldata, aux, ITEMNAME_SIZE );
        se.auxkilldata[ ITEMNAME_SIZE - 1 ] = '\0';
    }

    if ((death_type == KILLED_BY_MONSTER || death_type == KILLED_BY_BEAM)  
        && death_source >= 0 && death_source < MAX_MONSTERS)
    {
        struct monsters *monster = &menv[death_source];

        if (monster->type > 0 || monster->type <= NUM_MONSTERS) 
        { 
            se.death_source = monster->type;
            se.mon_num = monster->number;

            // Previously the weapon was only used for dancing weapons,
            // but now we pass it in as a string through the scorefile
            // entry to be appended in hiscores_format_single in long or
            // medium scorefile formats.
            // It still isn't used in monam for anything but flying weapons
            // though
            if (death_type == KILLED_BY_MONSTER 
                && monster->inv[MSLOT_WEAPON] != NON_ITEM)
            {
#if HISCORE_WEAPON_DETAIL
                set_ident_flags( mitm[monster->inv[MSLOT_WEAPON]],
                                 ISFLAG_IDENT_MASK );
#else
                // changing this to ignore the pluses to keep it short
                unset_ident_flags( mitm[monster->inv[MSLOT_WEAPON]],
                                   ISFLAG_IDENT_MASK );

                set_ident_flags( mitm[monster->inv[MSLOT_WEAPON]], 
                                 ISFLAG_KNOW_TYPE );

                // clear "runed" description text to make shorter yet
                set_equip_desc( mitm[monster->inv[MSLOT_WEAPON]], 0 );
#endif

                // Setting this is redundant for dancing weapons, however
                // we do care about the above indentification. -- bwr
                if (monster->type != MONS_DANCING_WEAPON) 
                {
                    it_name( monster->inv[MSLOT_WEAPON], DESC_NOCAP_A, info );
                    strncpy( se.auxkilldata, info, ITEMNAME_SIZE );
                    se.auxkilldata[ ITEMNAME_SIZE - 1 ] = '\0';
                }
            }
            
            strcpy( info, 
                    monam( monster->number, monster->type, true, DESC_NOCAP_A, 
                           monster->inv[MSLOT_WEAPON] ) );

            strncpy( se.death_source_name, info, 40 );
            se.death_source_name[39] = '\0';
        }
    }
    else
    {
        se.death_source = death_source;
        se.mon_num = 0;
        se.death_source_name[0] = '\0';
    }

    se.damage = dam;
    se.final_hp = you.hp;
    se.final_max_hp = you.hp_max;
    se.final_max_max_hp = you.hp_max + player_rotted();
    se.str = you.strength;
    se.intel = you.intel;
    se.dex = you.dex;

    se.god = you.religion;
    if (you.religion != GOD_NO_GOD)
    {
        se.piety = you.piety;
        se.penance = you.penance[you.religion];
    }

    // main dungeon: level is simply level
    se.dlvl = you.your_level + 1;
    switch (you.where_are_you)
    {
        case BRANCH_ORCISH_MINES:
        case BRANCH_HIVE:
        case BRANCH_LAIR:
        case BRANCH_SLIME_PITS:
        case BRANCH_VAULTS:
        case BRANCH_CRYPT:
        case BRANCH_HALL_OF_BLADES:
        case BRANCH_HALL_OF_ZOT:
        case BRANCH_ECUMENICAL_TEMPLE:
        case BRANCH_SNAKE_PIT:
        case BRANCH_ELVEN_HALLS:
        case BRANCH_TOMB:
        case BRANCH_SWAMP:
            se.dlvl = you.your_level - you.branch_stairs[you.where_are_you - 10];
            break;

        case BRANCH_DIS:
        case BRANCH_GEHENNA:
        case BRANCH_VESTIBULE_OF_HELL:
        case BRANCH_COCYTUS:
        case BRANCH_TARTARUS:
        case BRANCH_INFERNO:
        case BRANCH_THE_PIT:
            se.dlvl = you.your_level - 26;
            break;
    }

    se.branch = you.where_are_you;      // no adjustments necessary.
    se.level_type = you.level_type;     // pandemonium, labyrinth, dungeon..

    se.birth_time = you.birth_time;     // start time of game
    se.death_time = time( NULL );         // end time of game

    if (you.real_time != -1)
        se.real_time = you.real_time + (se.death_time - you.start_time);
    else 
        se.real_time = -1;

    se.num_turns = you.num_turns;

#ifdef WIZARD
    se.wiz_mode = (you.wizard ? 1 : 0);
#else
    se.wiz_mode = 0;
#endif

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
    char del_file[300];         // massive overkill!
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
                make_filename( info, you.your_name, level, dungeon,
                               false, false );
                unlink(info);
            }
        }
    }

    // temp level, if any
    make_filename( info, you.your_name, 0, 0, true, false );
    unlink(info);

    // create base file name
#ifdef SAVE_DIR_PATH
    snprintf( info, INFO_SIZE, SAVE_DIR_PATH "%s%d", you.your_name, (int) getuid());
#else
    strncpy(info, you.your_name, kFileNameLen);
    info[kFileNameLen] = '\0';
#endif

    // this is to catch the game package if it still exists.
#ifdef PACKAGE_SUFFIX
    strcpy(del_file, info);
    strcat(del_file, "." PACKAGE_SUFFIX);
    unlink(del_file);
#endif

    // last, but not least, delete player .sav file
    strcpy(del_file, info);
    strcat(del_file, ".sav");
    unlink(del_file);

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

    invent( -1, !dead );
    clrscr();

    if (!dump_char( "morgue.txt", !dead ))
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
    cprintf( "Goodbye, " );
    cprintf( you.your_name );
    cprintf( "." );
    cprintf( EOL EOL "    " ); // Space padding where # would go in list format

    char scorebuff[ HIGHSCORE_SIZE ];

    const int lines = hiscores_format_single_long( scorebuff, se, true );

    // truncate
    scorebuff[ HIGHSCORE_SIZE - 1 ] = '\0';
    cprintf( scorebuff );

    cprintf( EOL "Best Crawlers -" EOL );

    // "- 5" gives us an extra line in case the description wraps on a line.
    hiscores_print_list( get_number_of_lines() - lines - 5 );

    // just to pause, actual value returned does not matter {dlb}
    get_ch();
    end(0);
}
