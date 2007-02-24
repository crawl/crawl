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

#include "format.h"
#include "fight.h"
#include "initfile.h"
#include "itemname.h"
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

void update_turn_count()
{
    // Don't update turn counter when running/resting/traveling to
    // prevent pointless screen updates.
    if (!Options.show_turns
        || you.running > 0
        || (you.running < 0 && Options.travel_delay == -1))
    {
        return;
    }

    // FIXME: Create some kind of layout manager class so we can
    // templatise the heads-up display layout and stop hardcoding
    // these coords.
    gotoxy(61, 10);
    textcolor(LIGHTGREY);

    // Show the turn count starting from 1. You can still quit on turn 0.
    cprintf("%ld", you.num_turns);
}

void print_stats(void)
{
    textcolor(LIGHTGREY);

    if (you.redraw_hit_points)
    {
        const int max_max_hp = you.hp_max + player_rotted();
        int hp_percent;
        if ( max_max_hp )
            hp_percent = (you.hp * 100) / max_max_hp;
        else
            hp_percent = 100;

        for ( unsigned int i = 0; i < Options.hp_colour.size(); ++i )
        {
            if ( i+1 == Options.hp_colour.size() ||
                 hp_percent > Options.hp_colour[i+1].first )
            {
                textcolor(Options.hp_colour[i].second);
                break;
            }
        }
        
        gotoxy(44, 3);

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
        int mp_percent;
        if ( you.max_magic_points )
            mp_percent = (you.magic_points * 100) / you.max_magic_points;
        else
            mp_percent = 100;
        for ( unsigned int i = 0; i < Options.mp_colour.size(); ++i )
        {
            if ( i+1 == Options.mp_colour.size() ||
                 mp_percent > Options.mp_colour[i+1].first )
            {
                textcolor(Options.mp_colour[i].second);
                break;
            }
        }
        gotoxy(47, 4);

        cprintf( "%d", you.magic_points);

        textcolor(LIGHTGREY);
        cprintf("/%d", you.max_magic_points );

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
            cprintf( " (%d)  ", you.max_strength );
        else
            cprintf( "       " );

        you.redraw_strength = 0;

        if (you.strength < 1)
            ouch(INSTANT_DEATH, 0, KILLED_BY_WEAKNESS);

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
            cprintf( " (%d)  ", you.max_intel );
        else
            cprintf( "       " );

        you.redraw_intelligence = 0;

        if (you.intel < 1)
            ouch(INSTANT_DEATH, 0, KILLED_BY_STUPIDITY);
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
            cprintf( " (%d)  ", you.max_dex );
        else
            cprintf( "       " );

        you.redraw_dexterity = 0;

        if (you.dex < 1)
            ouch(INSTANT_DEATH, 0, KILLED_BY_CLUMSINESS);
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
        cprintf( "%-8d", you.gold );
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
            str_pass[39] = 0;

            cprintf("%s", str_pass);
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

        if (you.poisoning)
        {
            // We skip marking "quite" poisoned and instead mark the 
            // levels where the rules for dealing poison damage change
            // significantly.  See acr.cc for that code. -- bwr
            textcolor( bad_ench_colour( you.poisoning, 5, 10 ) );
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

const char* itosym1(int stat)
{
    return ( (stat >= 1) ? "+  " : ".  " );
}

const char* itosym3(int stat)
{
    return ( (stat >=  3) ? "+ + +" :
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

static const char* determine_color_string( int level ) {
    switch ( level ) {
    case 3:
    case 2:
	return "<lightgreen>";
    case 1:
        return "<green>";
    case -1:
        return "<red>";
    case -2:
    case -3:
        return "<lightred>";
    default:
        return "<lightgrey>";
    }
}

std::vector<formatted_string> get_full_detail(bool calc_unid)
{
    char buf[1000];
    // 3 columns, splits at columns 32, 52
    column_composer cols(3, 32, 52);
    char god_colour_tag[20];
    god_colour_tag[0] = 0;
    std::string godpowers(god_name(you.religion));
    if ( you.religion != GOD_NO_GOD )
    {
        if ( player_under_penance() )
            strcpy(god_colour_tag, "<red>*");
        else
        {
            snprintf(god_colour_tag, sizeof god_colour_tag, "<%s>",
                     colour_to_str(god_colour(you.religion)));
            // piety rankings
            int prank = piety_rank() - 1;
            if ( prank < 0 )
                prank = 0;
            // Careful about overflow. We erase some of the god's name
            // if necessary.
            godpowers = godpowers.substr(0, 17 - prank) +
                std::string(prank, '*');
        }
    }
    snprintf(buf, sizeof buf,
             "<yellow>%s the %s</yellow>\n\n"
             "Race       : %s\n"
             "Class      : %s\n"
             "Worship    : %s%s\n"
             "Level      : %7d\n"
             "Exp        : %7lu\n",
             you.your_name, player_title(),
             species_name(you.species,you.experience_level),
             you.class_name,
             god_colour_tag, godpowers.c_str(),
             you.experience_level,
             you.experience);
    cols.add_formatted(0, buf, false);

    if (you.experience_level < 27)
    {
        int xp_needed = (exp_needed(you.experience_level+2)-you.experience)+1;
        snprintf(buf, sizeof buf,
                 "Next Level : %7lu\n"
                 "Exp Needed : %7d\n",
                 exp_needed(you.experience_level + 2) + 1,
                 xp_needed);
        cols.add_formatted(0, buf, false);
    }
    else
        cols.add_formatted(0, "\n\n", false);
       
    snprintf(buf, sizeof buf,
             "Spls.Left  : %7d\n"
             "Gold       : %7d\n",
             player_spell_levels(),
             you.gold);
    cols.add_formatted(0, buf, false);

    if (!player_rotted())
    {
        if (you.hp < you.hp_max)
            snprintf(buf, sizeof buf, "HP         : %3d/%d",you.hp,you.hp_max);
        else
            snprintf(buf, sizeof buf, "HP         : %3d", you.hp);
    }
    else
    {
        snprintf(buf, sizeof buf, "HP         : %3d/%d (%d)", 
                 you.hp, you.hp_max, you.hp_max + player_rotted() );
    }
    cols.add_formatted(0, buf, true);

    if (you.magic_points < you.max_magic_points)
        snprintf(buf, sizeof buf, "MP         : %3d/%d", 
                 you.magic_points, you.max_magic_points);
    else
        snprintf(buf, sizeof buf, "MP         : %3d", you.magic_points);
    cols.add_formatted(0, buf, false);

    if (you.strength == you.max_strength)
        snprintf(buf, sizeof buf, "Str        : %3d", you.strength);
    else
        snprintf(buf, sizeof buf, "Str        : <yellow>%3d</yellow> (%d)",
                 you.strength, you.max_strength);
    cols.add_formatted(0, buf, false);
    
    if (you.intel == you.max_intel)
        snprintf(buf, sizeof buf, "Int        : %3d", you.intel);
    else
        snprintf(buf, sizeof buf, "Int        : <yellow>%3d</yellow> (%d)",
                 you.intel, you.max_intel);
    cols.add_formatted(0, buf, false);
    
    if (you.dex == you.max_dex)
        snprintf(buf, sizeof buf, "Dex        : %3d", you.dex);
    else
        snprintf(buf, sizeof buf, "Dex        : <yellow>%3d</yellow> (%d)",
                 you.dex, you.max_dex);
    cols.add_formatted(0, buf, false);

    snprintf(buf, sizeof buf,
             "AC         : %3d\n"
             "Evasion    : %3d\n"
             "Shield     : %3d\n",
             player_AC(),
             player_evasion(),
             player_shield_class());
    cols.add_formatted(0, buf, false);

    if (you.real_time != -1)
    {
        const time_t curr = you.real_time + (time(NULL) - you.start_time);
        char buff[200];
        make_time_string( curr, buff, sizeof(buff), true );

        snprintf(buf, sizeof buf,
                 "Play time  : %10s\n"
                 "Turns      : %10ld\n",
                 buff, you.num_turns );
        cols.add_formatted(0, buf, true);
    }

    const int rfire = player_res_fire(calc_unid);
    const int rcold = player_res_cold(calc_unid);
    const int rlife = player_prot_life(calc_unid);
    const int rpois = player_res_poison(calc_unid);
    const int relec = player_res_electricity(calc_unid);

    snprintf(buf, sizeof buf, "\n\n"             
             "%sRes.Fire  : %s\n"
             "%sRes.Cold  : %s\n"
             "%sLife Prot.: %s\n"
             "%sRes.Poison: %s\n"
             "%sRes.Elec. : %s\n",
             determine_color_string(rfire), itosym3(rfire),
             determine_color_string(rcold), itosym3(rcold),
             determine_color_string(rlife), itosym3(rlife),
             determine_color_string(rpois), itosym1(rpois),
             determine_color_string(relec), itosym1(relec));
    cols.add_formatted(1, buf, false);

    const int rsust = player_sust_abil(calc_unid);
    const int rmuta = wearing_amulet(AMU_RESIST_MUTATION, calc_unid);
    const int rslow = wearing_amulet(AMU_RESIST_SLOW, calc_unid);
    const int rclar = wearing_amulet(AMU_CLARITY, calc_unid);

    snprintf(buf, sizeof buf,
             "%sSust.Abil.: %s\n"
             "%sRes.Mut.  : %s\n"
             "%sRes.Slow  : %s\n"
             "%sClarity   : %s\n \n",
             determine_color_string(rsust), itosym1(rsust),
             determine_color_string(rmuta), itosym1(rmuta),
             determine_color_string(rslow), itosym1(rslow),
             determine_color_string(rclar), itosym1(rclar));
    cols.add_formatted(1, buf, true);

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
            else if (eqslot == EQ_BOOTS &&
                     (you.species == SP_CENTAUR || you.species == SP_NAGA))
                slot = "Barding";

            if ( you.equip[ e_order[i] ] != -1)
            {
                const int inum = you.equip[e_order[i]];
                in_name( inum, DESC_PLAIN, str_pass, true );
                str_pass[38] = 0; // truncate
                const char* colname = colour_to_str(you.inv[inum].colour);
                snprintf(buf, sizeof buf, "%-7s: <%s>%s</%s>",
                         slot, colname, str_pass, colname);
            }
            else
            {
                if (e_order[i] == EQ_WEAPON)
                {
                    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
                        snprintf(buf, sizeof buf, "%-7s: Blade Hands", slot);
                    else if (you.skills[SK_UNARMED_COMBAT])
                        snprintf(buf, sizeof buf, "%-7s: Unarmed", slot);
                    else
                        snprintf(buf, sizeof buf, "%-7s:", slot);
                }
                else
                {
                    snprintf(buf, sizeof buf, "%-7s:", slot);
                }
            }
            cols.add_formatted(1, buf, false);
        }
    }

    const int rinvi = player_see_invis(calc_unid);
    const int rward = wearing_amulet(AMU_WARDING, calc_unid) ||
        (you.religion == GOD_VEHUMET && you.duration[DUR_PRAYER] &&
         !player_under_penance() && you.piety >= piety_breakpoint(2));
    const int rcons = wearing_amulet(AMU_CONSERVATION, calc_unid);
    const int rcorr = wearing_amulet(AMU_RESIST_CORROSION, calc_unid);

    snprintf(buf, sizeof buf, "\n\n"
             "%sSee Invis. : %s\n"
             "%sWarding    : %s\n"
             "%sConserve   : %s\n"
             "%sRes.Corr.  : %s\n",
             determine_color_string(rinvi), itosym1(rinvi),
             determine_color_string(rward), itosym1(rward),
             determine_color_string(rcons), itosym1(rcons),
             determine_color_string(rcorr), itosym1(rcorr));
    cols.add_formatted(2, buf, false);    
             
    int saplevel = 0;
    switch (you.species)
    {            
    case SP_GHOUL:
        saplevel = 3;
        snprintf(buf, sizeof buf, "%sSaprovore  : %s",
                 determine_color_string(3), itosym3(3) );
        break;

    case SP_KOBOLD:
    case SP_TROLL:
        saplevel = 2;
        snprintf(buf, sizeof buf, "%sSaprovore  : %s",
                 determine_color_string(2), itosym3(2) );
        break;

    case SP_HILL_ORC:
    case SP_OGRE:
        saplevel = 1;
        break;
    default:
        saplevel = 0;
        break;
    }
    const char* pregourmand;
    const char* postgourmand;
    if ( wearing_amulet(AMU_THE_GOURMAND, calc_unid) )
    {
        pregourmand = "Gourmand   : ";
        postgourmand = itosym1(1);
        saplevel = 1;
    }
    else
    {
        pregourmand = "Saprovore  : ";
        postgourmand = itosym3(saplevel);
    }
    snprintf(buf, sizeof buf, "%s%s%s",
             determine_color_string(saplevel), pregourmand, postgourmand);
    cols.add_formatted(2, buf, false);

    cols.add_formatted(2, " \n", false);

    if ( scan_randarts(RAP_PREVENT_TELEPORTATION, calc_unid) )
        snprintf(buf, sizeof buf, "%sPrev.Telep.: %s",
                 determine_color_string(-1), itosym1(1));
    else
    {
        const int rrtel = player_teleport(calc_unid);
        snprintf(buf, sizeof buf, "%sRnd.Telep. : %s",
                 determine_color_string(rrtel), itosym1(rrtel));
    }
    cols.add_formatted(2, buf, false);

    const int rctel = player_control_teleport(calc_unid);
    const int rlevi = player_is_levitating();
    const int rcfli = wearing_amulet(AMU_CONTROLLED_FLIGHT, calc_unid);
    snprintf(buf, sizeof buf,
             "%sCtrl.Telep.: %s\n"
             "%sLevitation : %s\n"
             "%sCtrl.Flight: %s\n",
             determine_color_string(rctel), itosym1(rctel),
             determine_color_string(rlevi), itosym1(rlevi),
             determine_color_string(rcfli), itosym1(rcfli));
    cols.add_formatted(2, buf, false);

    return cols.formatted_lines();
}
