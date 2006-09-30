/*
 *  File:       output.cc
 *  Summary:    Functions used to print player related info.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <2>      5/20/99        BWR             Efficiency changes for curses.
 *      <1>      -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "output.h"

#include <stdlib.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "fight.h"
#include "itemname.h"
#include "menu.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "skills2.h"
#include "stuff.h"

static int bad_ench_colour( int lvl, int orange, int red )
{
    if (lvl > red)
        return (RED);
    else if (lvl > orange)
        return (LIGHTRED);

    return (YELLOW);
}

static void dur_colour( int colour, bool running_out )
{
    if (running_out)
        textcolor( colour );
    else
    {
        switch (colour)
        { 
        case GREEN:     textcolor( LIGHTGREEN );        break;
        case BLUE:      textcolor( LIGHTBLUE );         break;
        case MAGENTA:   textcolor( LIGHTMAGENTA );      break;
        case LIGHTGREY: textcolor( WHITE );             break;
        }
    }
}

void print_stats(void)
{
    textcolor(LIGHTGREY);

    if (you.redraw_hit_points)
    {
        const int max_max_hp = you.hp_max + player_rotted();
        const int hp_warn = MAXIMUM( 25, Options.hp_warning );
        const int hp_attent = MAXIMUM( 10, Options.hp_attention );

        gotoxy(44, 3);

        if (you.hp <= (you.hp_max * hp_warn) / 100)
            textcolor(RED);
        else if (you.hp <= (you.hp_max * hp_attent) / 100)
            textcolor(YELLOW);

        cprintf( "%d", you.hp );

        textcolor(LIGHTGREY);
        cprintf( "/%d", you.hp_max );

        if (max_max_hp != you.hp_max)
            cprintf( " (%d)", max_max_hp );

#ifdef UNIX
        clear_to_end_of_line();
#else
        cprintf("        ");
#endif

        you.redraw_hit_points = 0;
    }

    if (you.redraw_magic_points)
    {
        gotoxy(47, 4);

        cprintf( "%d/%d", you.magic_points, you.max_magic_points );

#ifdef UNIX
        clear_to_end_of_line();
#else
        cprintf("        ");
#endif

        you.redraw_magic_points = 0;
    }

    if (you.redraw_strength)
    {
        if (you.strength < 0)
            you.strength = 0;
        else if (you.strength > 72)
            you.strength = 72;

        if (you.max_strength > 72)
            you.max_strength = 72;

        gotoxy(45, 7);

        if (you.might)
            textcolor(LIGHTBLUE);  // no end of effect warning 
        else if (you.strength < you.max_strength)
            textcolor(YELLOW);

        cprintf( "%d", you.strength );
        textcolor(LIGHTGREY);

        if (you.strength != you.max_strength)
            cprintf( " (%d)", you.max_strength );
        else
            cprintf( "       " );

        you.redraw_strength = 0;

        if (you.strength < 1)
            ouch(-9999, 0, KILLED_BY_WEAKNESS);

        burden_change();
    }

    if (you.redraw_intelligence)
    {
        if (you.intel < 0)
            you.intel = 0;
        else if (you.intel > 72)
            you.intel = 72;

        if (you.max_intel > 72)
            you.max_intel = 72;

        gotoxy(45, 8);

        if (you.intel < you.max_intel)
            textcolor(YELLOW);

        cprintf( "%d", you.intel );
        textcolor(LIGHTGREY);

        if (you.intel != you.max_intel)
            cprintf( " (%d)", you.max_intel );
        else
            cprintf( "       " );

        you.redraw_intelligence = 0;

        if (you.intel < 1)
            ouch(-9999, 0, KILLED_BY_STUPIDITY);
    }

    if (you.redraw_dexterity)
    {
        if (you.dex < 0)
            you.dex = 0;
        else if (you.dex > 72)
            you.dex = 72;

        if (you.max_dex > 72)
            you.max_dex = 72;

        gotoxy(45, 9);

        if (you.dex < you.max_dex)
            textcolor(YELLOW);

        cprintf( "%d", you.dex );
        textcolor(LIGHTGREY);

        if (you.dex != you.max_dex)
            cprintf( " (%d)", you.max_dex );
        else
            cprintf( "       " );

        you.redraw_dexterity = 0;

        if (you.dex < 1)
            ouch(-9999, 0, KILLED_BY_CLUMSINESS);
    }

    if (you.redraw_armour_class)
    {
        gotoxy(44, 5);

        if (you.duration[DUR_STONEMAIL])
            dur_colour( BLUE, (you.duration[DUR_STONEMAIL] <= 6) );
        else if (you.duration[DUR_ICY_ARMOUR] || you.duration[DUR_STONESKIN])
            textcolor( LIGHTBLUE );  // no end of effect warning

        cprintf( "%d  ", player_AC() );
        textcolor( LIGHTGREY );

        gotoxy(50, 5);

        if (you.duration[DUR_CONDENSATION_SHIELD])      //jmf: added 24mar2000
            textcolor( LIGHTBLUE );  // no end of effect warning

        cprintf( "(%d) ", player_shield_class() );
        textcolor( LIGHTGREY );

        you.redraw_armour_class = 0;
    }

    if (you.redraw_evasion)
    {
        gotoxy(44, 6);

        if (you.duration[DUR_FORESCRY])
            textcolor(LIGHTBLUE);  // no end of effect warning

        cprintf( "%d  ", player_evasion() );
        textcolor(LIGHTGREY);

        you.redraw_evasion = 0;
    }

    if (you.redraw_gold)
    {
        gotoxy(46, 10);
        cprintf( "%d     ", you.gold );
        you.redraw_gold = 0;
    }

    if (you.redraw_experience)
    {
        gotoxy(52, 11);

#if DEBUG_DIAGNOSTICS
        cprintf( "%d/%lu  (%d/%d)", 
                 you.experience_level, you.experience, 
                 you.skill_cost_level, you.exp_available );
#else
        cprintf( "%d/%lu  (%d)", 
                 you.experience_level, you.experience, you.exp_available );
#endif

#ifdef UNIX
        clear_to_end_of_line();
#else
        cprintf("   ");
#endif
        you.redraw_experience = 0;
    }

    if (you.wield_change)
    {
        gotoxy(40, 13);
#ifdef UNIX
        clear_to_end_of_line();
#else
        cprintf("                                       ");
#endif

        if (you.equip[EQ_WEAPON] != -1)
        {
            gotoxy(40, 13);
            textcolor(you.inv[you.equip[EQ_WEAPON]].colour);

            char str_pass[ ITEMNAME_SIZE ];
            in_name( you.equip[EQ_WEAPON], DESC_INVENTORY, str_pass, false );
            int prefcol = menu_colour(str_pass);
            if (prefcol != -1)
                textcolor(prefcol);

            in_name( you.equip[EQ_WEAPON], DESC_INVENTORY, str_pass, 
                     Options.terse_hand );
            str_pass[39] = '\0';

            cprintf(str_pass);
            textcolor(LIGHTGREY);
        }
        else
        {
            gotoxy(40, 13);

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            {
                textcolor(RED);
                cprintf("Blade Hands");
                textcolor(LIGHTGREY);
            }
            else
            {
                textcolor(LIGHTGREY);
                cprintf("Nothing wielded");
            }
        }
        you.wield_change = false;
    }

    // The colour scheme for these flags is currently:
    //
    // - yellow, "orange", red      for bad conditions
    // - light grey, white          for god based conditions
    // - green, light green         for good conditions
    // - blue, light blue           for good enchantments
    // - magenta, light magenta     for "better" enchantments (deflect, fly)

    if (you.redraw_status_flags & REDRAW_LINE_1_MASK)
    {
        gotoxy(40, 14);

#ifdef UNIX
        clear_to_end_of_line();
#else
        cprintf( "                                       " );
        gotoxy(40, 14);
#endif

        switch (you.burden_state)
        {
        case BS_OVERLOADED:
            textcolor( RED );
            cprintf( "Overloaded " );
            break;

        case BS_ENCUMBERED:
            textcolor( LIGHTRED );
            cprintf( "Encumbered " );
            break;

        case BS_UNENCUMBERED:
            break;
        }

        switch (you.hunger_state)
        {
        case HS_ENGORGED:
            textcolor( LIGHTGREEN );
            cprintf( "Engorged" );
            break;

        case HS_FULL:
            textcolor( GREEN );
            cprintf( "Full" );
            break;

        case HS_SATIATED:
            break;

        case HS_HUNGRY:
            textcolor( YELLOW );
            cprintf( "Hungry" );
            break;

        case HS_STARVING:
            textcolor( RED );
            cprintf( "Starving" );
            break;
        }

        textcolor( LIGHTGREY );

#if DEBUG_DIAGNOSTICS
        // debug mode hunger-o-meter 
        cprintf( " (%d:%d) ", you.hunger - you.old_hunger, you.hunger );
#endif
    }

    if (you.redraw_status_flags & REDRAW_LINE_2_MASK)
    {
        gotoxy(40, 15);

#ifdef UNIX
        clear_to_end_of_line();
#else
        cprintf( "                                       " );
        gotoxy(40, 15);
#endif

        // Max length of this line = 8 * 5 - 1 = 39

        if (you.duration[DUR_PRAYER])
        {
            textcolor( WHITE );  // no end of effect warning
            cprintf( "Pray " );
        }

        if (you.duration[DUR_REPEL_UNDEAD])
        {
            dur_colour( LIGHTGREY, (you.duration[DUR_REPEL_UNDEAD] <= 4) );
            cprintf( "Holy " );
        }

        if (you.duration[DUR_DEFLECT_MISSILES])
        {

            dur_colour( MAGENTA, (you.duration[DUR_DEFLECT_MISSILES] <= 6) );
            cprintf( "DMsl " );
        }
        else if (you.duration[DUR_REPEL_MISSILES])
        {
            dur_colour( BLUE, (you.duration[DUR_REPEL_MISSILES] <= 6) );
            cprintf( "RMsl " );
        }

        if (you.duration[DUR_REGENERATION])
        {
            dur_colour( BLUE, (you.duration[DUR_REGENERATION] <= 6) );
            cprintf( "Regen " );
        }

        if (you.duration[DUR_INSULATION])
        {
            dur_colour( BLUE, (you.duration[DUR_INSULATION] <= 6) );
            cprintf( "Ins " );
        }

        if (player_is_levitating())
        {
            bool perm = (you.species == SP_KENKU && you.experience_level >= 15) 
                        || (player_equip_ego_type( EQ_BOOTS, SPARM_LEVITATION ))
                        || (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON);

            if (wearing_amulet( AMU_CONTROLLED_FLIGHT ))
            {
                dur_colour( MAGENTA, (you.levitation <= 10 && !perm) );
                cprintf( "Fly " );
            }
            else
            {
                dur_colour( BLUE, (you.levitation <= 10 && !perm) );
                cprintf( "Lev " );
            }
        }

        if (you.invis)
        {
            dur_colour( BLUE, (you.invis <= 6) );
            cprintf( "Invis " );
        }

        // Perhaps this should be reversed to show when it can be used? 
        // In that case, it should be probably be GREEN, and we'd have
        // to check to see if the player does have a breath weapon. -- bwr
        if (you.duration[DUR_BREATH_WEAPON])
        {
            textcolor( YELLOW );  // no warning
            cprintf( "BWpn" );
        }

        textcolor( LIGHTGREY );
    }

    if (you.redraw_status_flags & REDRAW_LINE_3_MASK)
    {
        gotoxy(40, 16);

#ifdef UNIX
        clear_to_end_of_line();
#else
        cprintf( "                                       " );
        gotoxy(40, 16);
#endif
        // Max length of this line = 7 * 5 + 3 - 1 = 37

        // Note the usage of bad_ench_colour() correspond to levels that
        // can be found in player.cc, ie those that the player can tell by 
        // using the '@' command.  Things like confusion and sticky flame 
        // hide their amounts and are thus always the same colour (so
        // we're not really exposing any new information). --bwr
        if (you.conf)
        {
            textcolor( RED );   // no different levels
            cprintf( "Conf " );
        }

        if (you.duration[DUR_LIQUID_FLAMES])
        {
            textcolor( RED );   // no different levels
            cprintf( "Fire " );
        }

        if (you.poison)
        {
            // We skip marking "quite" poisoned and instead mark the 
            // levels where the rules for dealing poison damage change
            // significantly.  See acr.cc for that code. -- bwr
            textcolor( bad_ench_colour( you.poison, 5, 10 ) );
            cprintf( "Pois " );
        }

        if (you.disease)
        {
            textcolor( bad_ench_colour( you.disease, 40, 120 ) );
            cprintf( "Sick " );
        }

        if (you.rotting)
        {
            textcolor( bad_ench_colour( you.rotting, 4, 8 ) );
            cprintf( "Rot " );
        }

        if (you.magic_contamination >= 5)
        {
            textcolor( bad_ench_colour( you.magic_contamination, 15, 25 ) );
            cprintf( "Glow " );
        }

        if (you.duration[DUR_SWIFTNESS])
        {
            dur_colour( BLUE, (you.duration[DUR_SWIFTNESS] <= 6) );
            cprintf( "Swift " );
        }

        if (you.slow && !you.haste)
        {
            textcolor( RED );  // no end of effect warning
            cprintf( "Slow" );
        }
        else if (you.haste && !you.slow)
        {
            dur_colour( BLUE, (you.haste <= 6) );
            cprintf( "Fast" );
        }

        textcolor( LIGHTGREY );
    }

    you.redraw_status_flags = 0;

#if DEBUG_DIAGNOSTICS
    // debug mode GPS
    gotoxy(40, 17);
    cprintf( "Position (%2d,%2d)", you.x_pos, you.y_pos );
#endif

#ifdef UNIX
    // get curses to redraw screen
    update_screen();
#endif
}                               // end print_stats()

unsigned char* itosym1(int stat)
{
    return (unsigned char*)( (stat >= 1) ? "+  " : ".  " );
}

unsigned char* itosym3(int stat)
{
    return (unsigned char*)( (stat >= 3) ? "+ + +" :
             (stat ==  2) ? "+ + ." :
             (stat ==  1) ? "+ . ." :
             (stat ==  0) ? ". . ." :
	     (stat == -1) ? "x . ." :
	     (stat == -2) ? "x x ." :
                            "x x x");
}

static const char *s_equip_slot_names[] = 
{
    "Weapon", 
    "Cloak",
    "Helmet",
    "Gloves",
    "Boots",
    "Shield",
    "Armour",
    "Left Ring",
    "Right Ring",
    "Amulet",
};

const char *equip_slot_to_name(int equip)
{
    if (equip == EQ_RINGS)
        return "Ring";
    if (equip < 0 || equip >= NUM_EQUIP)
        return "";
    return s_equip_slot_names[equip];
}

int equip_name_to_slot(const char *s)
{
    for (int i = 0; i < NUM_EQUIP; ++i)
    {
        if (!stricmp(s_equip_slot_names[i], s))
            return i;
    }
    return -1;
}

void get_full_detail(char* buffer, bool calc_unid)
{
#define FIR_AD buffer,44
#define CUR_AD &buffer[++lines*45],44
#define BUF_SIZE 25*3*45
    int lines = 0;

    memset(buffer, 0, BUF_SIZE);

    snprintf(CUR_AD, "%s the %s", you.your_name, player_title());
    lines++;
    snprintf(CUR_AD, "Race       : %s", species_name(you.species,you.experience_level) );
    snprintf(CUR_AD, "Class      : %s", you.class_name);
    snprintf(CUR_AD, "Worship    : %s", 
            you.religion == GOD_NO_GOD? "" : god_name(you.religion) );
    snprintf(CUR_AD, "Level      : %7d", you.experience_level);
    snprintf(CUR_AD, "Exp        : %7lu", you.experience);

    if (you.experience_level < 27)
    {
        int xp_needed = (exp_needed(you.experience_level+2) - you.experience) + 1;
        snprintf(CUR_AD, "Next Level : %7lu", exp_needed(you.experience_level + 2) + 1);
        snprintf(CUR_AD, "Exp Needed : %7d", xp_needed);
    }
    else
    {
        snprintf(CUR_AD, "                  ");
        snprintf(CUR_AD, "                  ");
    }

    snprintf(CUR_AD, "Spls.Left  : %7d", player_spell_levels() );
    snprintf(CUR_AD, "Gold       : %7d", you.gold );

    lines++;

    if (!player_rotted())
    {
        if (you.hp < you.hp_max)
            snprintf(CUR_AD, "HP         : %3d/%d", you.hp, you.hp_max);
        else
            snprintf(CUR_AD, "HP         : %3d", you.hp);
    }
    else
    {
        snprintf(CUR_AD,  "HP         : %3d/%d (%d)", 
                you.hp, you.hp_max, you.hp_max + player_rotted() );
    }

    if (you.magic_points < you.max_magic_points)
        snprintf(CUR_AD, "MP         : %3d/%d", 
            you.magic_points, you.max_magic_points);
    else
        snprintf(CUR_AD, "MP         : %3d", you.magic_points);

    if (you.strength == you.max_strength)
        snprintf(CUR_AD, "Str        : %3d", you.strength);
    else
        snprintf(CUR_AD, "Str        : %3d (%d)", 
                you.strength, you.max_strength);
    
    if (you.intel == you.max_intel)
        snprintf(CUR_AD, "Int        : %3d", you.intel);
    else
        snprintf(CUR_AD, "Int        : %3d (%d)", you.intel, you.max_intel);
    
    if (you.dex == you.max_dex)
        snprintf(CUR_AD, "Dex        : %3d", you.dex);
    else
        snprintf(CUR_AD, "Dex        : %3d (%d)", you.dex, you.max_dex);

    snprintf(CUR_AD, "AC         : %3d", player_AC() );
    snprintf(CUR_AD, "Evasion    : %3d", player_evasion() );
    snprintf(CUR_AD, "Shield     : %3d", player_shield_class() );
    lines++;

    if (you.real_time != -1)
    {
        const time_t curr = you.real_time + (time(NULL) - you.start_time);
        char buff[200];
        make_time_string( curr, buff, sizeof(buff), true );

        snprintf(CUR_AD, "Play time  : %10s", buff);
        snprintf(CUR_AD, "Turns      : %10ld", you.num_turns );
    }

    lines = 27;

    snprintf(CUR_AD, "Res.Fire  : %s", 
            itosym3( player_res_fire(calc_unid) ) );
    snprintf(CUR_AD, "Res.Cold  : %s", 
            itosym3( player_res_cold(calc_unid) ) );
    snprintf(CUR_AD, "Life Prot.: %s", 
            itosym3( player_prot_life(calc_unid) ) );
    snprintf(CUR_AD, "Res.Poison: %s", 
            itosym1( player_res_poison(calc_unid) ) );
    snprintf(CUR_AD, "Res.Elec. : %s", 
            itosym1( player_res_electricity(calc_unid) ) );
    lines++;
    
    snprintf(CUR_AD, "Sust.Abil.: %s", 
            itosym1( player_sust_abil(calc_unid) ) );
    snprintf(CUR_AD, "Res.Mut.  : %s", 
            itosym1( wearing_amulet( AMU_RESIST_MUTATION, calc_unid) ) );
    snprintf(CUR_AD, "Res.Slow  : %s", 
            itosym1( wearing_amulet( AMU_RESIST_SLOW, calc_unid) ) );
    snprintf(CUR_AD, "Clarity   : %s", 
            itosym1( wearing_amulet( AMU_CLARITY, calc_unid) ) );
    lines++;
    lines++;

    {
        char str_pass[ITEMNAME_SIZE];
        const int e_order[] = 
        {
            EQ_WEAPON, EQ_BODY_ARMOUR, EQ_SHIELD, EQ_HELMET, EQ_CLOAK,
            EQ_GLOVES, EQ_BOOTS, EQ_AMULET, EQ_RIGHT_RING, EQ_LEFT_RING
        };

        for(int i = 0; i < NUM_EQUIP; i++)
        {
            int eqslot = e_order[i];
            const char *slot = equip_slot_to_name( eqslot );
            if (eqslot == EQ_LEFT_RING || eqslot == EQ_RIGHT_RING)
                slot = "Ring";
            else if (eqslot == EQ_BOOTS 
                    && (you.species == SP_CENTAUR
                        || you.species == SP_NAGA))
                slot = "Barding";

            if ( you.equip[ e_order[i] ] != -1)
            {
                in_name( you.equip[ e_order[i] ], DESC_PLAIN, 
                         str_pass, true );
                snprintf(CUR_AD, "%-7s: %s", slot, str_pass);
            }
            else
            {
                if (e_order[i] == EQ_WEAPON)
                {
                    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
                        snprintf(CUR_AD, "%-7s: Blade Hands", slot);
                    else if (you.skills[SK_UNARMED_COMBAT])
                        snprintf(CUR_AD, "%-7s: Unarmed", slot);
                    else
                        snprintf(CUR_AD, "%-7s:", slot);
                }
                else
                {
                    snprintf(CUR_AD, "%-7s:", slot);
                }
            }
        }
    }

    lines = 52;
    snprintf(CUR_AD, "See Invis. : %s", 
            itosym1( player_see_invis(calc_unid) ) );
    snprintf(CUR_AD, "Warding    : %s", 
            itosym1( wearing_amulet(AMU_WARDING, calc_unid)
                     || (you.religion == GOD_VEHUMET && 
                         you.duration[DUR_PRAYER] && 
                         !player_under_penance() &&
                         you.piety >= 75) ) );
    snprintf(CUR_AD, "Conserve   : %s", 
            itosym1( wearing_amulet( AMU_CONSERVATION, calc_unid) ) );
    snprintf(CUR_AD, "Res.Corr.  : %s", 
            itosym1( wearing_amulet( AMU_RESIST_CORROSION, calc_unid) ) );
    
    if ( !wearing_amulet( AMU_THE_GOURMAND, calc_unid) )
    {
        switch (you.species)
        {
        case SP_GHOUL:
            snprintf(CUR_AD, "Saprovore  : %s", itosym3(3) );
            break;

        case SP_KOBOLD:
        case SP_TROLL:
            snprintf(CUR_AD, "Saprovore  : %s", itosym3(2) );
            break;

        case SP_HILL_ORC:
        case SP_OGRE:
            snprintf(CUR_AD, "Saprovore  : %s", itosym3(1) );
            break;
#ifdef V_FIX
        case SP_OGRE_MAGE:
            snprintf(CUR_AD, "Voracious  : %s", itosym1(1) );
            break;
#endif
        default:
            snprintf(CUR_AD, "Gourmand   : %s", itosym1(0) );
            break;
        }
    }
    else
    {
        snprintf(CUR_AD, "Gourmand   : %s", 
                itosym1( wearing_amulet( AMU_THE_GOURMAND, calc_unid) ) );
    }
    
    lines++;

    if ( scan_randarts(RAP_PREVENT_TELEPORTATION, calc_unid) )
        snprintf(CUR_AD, "Prev.Telep.: %s", 
            itosym1( scan_randarts(RAP_PREVENT_TELEPORTATION, calc_unid) ) );
    else
        snprintf(CUR_AD, "Rnd.Telep. : %s", 
            itosym1( player_teleport(calc_unid) ) );
    snprintf(CUR_AD, "Ctrl.Telep.: %s", 
	     itosym1( player_control_teleport(calc_unid) ) );
    snprintf(CUR_AD, "Levitation : %s", itosym1( player_is_levitating() ) );
    snprintf(CUR_AD, "Ctrl.Flight: %s", 
            itosym1( wearing_amulet(AMU_CONTROLLED_FLIGHT, calc_unid) ) ); 
    lines++;

    return;
}
