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

#include "abl-show.h"
#include "describe.h"
#include "direct.h"
#include "format.h"
#include "fight.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "menu.h"
#include "message.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "skills2.h"
#include "stuff.h"
#include "transfor.h"

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

#ifdef DGL_SIMPLE_MESSAGING
void update_message_status()
{
    textcolor(LIGHTBLUE);

    gotoxy(crawl_view.hudp.x + 35, 2);
    if (SysEnv.have_messages)
        cprintf("(msg)");
    else
        cprintf("     ");
    textcolor(LIGHTGREY);
}
#endif

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
    gotoxy(crawl_view.hudp.x + 21, 10);
    textcolor(LIGHTGREY);

    // Show the turn count starting from 1. You can still quit on turn 0.
    cprintf("%ld", you.num_turns);
}

void print_stats(void)
{
    textcolor(LIGHTGREY);

    const int xcol = crawl_view.hudp.x;
    
    // Displayed evasion is now tied to dex.
    if (you.redraw_dexterity)
        you.redraw_evasion = true;

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

        gotoxy(xcol + 4, 3);

        cprintf( "%d", you.hp );

        textcolor(LIGHTGREY);
        cprintf( "/%d", you.hp_max );

        if (max_max_hp != you.hp_max)
            cprintf( " (%d)", max_max_hp );

        clear_to_end_of_line();
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
        gotoxy(xcol + 7, 4);

        cprintf( "%d", you.magic_points);

        textcolor(LIGHTGREY);
        cprintf("/%d", you.max_magic_points );

        clear_to_end_of_line();
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

        gotoxy(xcol + 5, 7);

        if (you.duration[DUR_MIGHT])
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

        gotoxy(xcol + 5, 8);

        if (you.intel < you.max_intel)
            textcolor(YELLOW);

        cprintf( "%d", you.intel );
        textcolor(LIGHTGREY);

        if (you.intel != you.max_intel)
            cprintf( " (%d)  ", you.max_intel );
        else
            cprintf( "       " );

        you.redraw_intelligence = 0;
    }

    if (you.redraw_dexterity)
    {
        if (you.dex < 0)
            you.dex = 0;
        else if (you.dex > 72)
            you.dex = 72;

        if (you.max_dex > 72)
            you.max_dex = 72;

        gotoxy(xcol + 5, 9);

        if (you.dex < you.max_dex)
            textcolor(YELLOW);

        cprintf( "%d", you.dex );
        textcolor(LIGHTGREY);

        if (you.dex != you.max_dex)
            cprintf( " (%d)  ", you.max_dex );
        else
            cprintf( "       " );

        you.redraw_dexterity = 0;
    }

    if (you.redraw_armour_class)
    {
        gotoxy(xcol + 4, 5);

        if (you.duration[DUR_STONEMAIL])
            dur_colour( BLUE, (you.duration[DUR_STONEMAIL] <= 6) );
        else if (you.duration[DUR_ICY_ARMOUR] || you.duration[DUR_STONESKIN])
            textcolor( LIGHTBLUE );  // no end of effect warning

        cprintf( "%d  ", player_AC() );
        textcolor( LIGHTGREY );

        gotoxy(xcol + 10, 5);

        if (you.duration[DUR_CONDENSATION_SHIELD])      //jmf: added 24mar2000
            textcolor( LIGHTBLUE );  // no end of effect warning

        cprintf( "(%d) ", player_shield_class() );
        textcolor( LIGHTGREY );

        you.redraw_armour_class = 0;
    }

    if (you.redraw_evasion)
    {
        gotoxy(xcol + 4, 6);

        if (you.duration[DUR_FORESCRY])
            textcolor(LIGHTBLUE);  // no end of effect warning

        cprintf( "%d  ", player_evasion() );
        textcolor(LIGHTGREY);

        you.redraw_evasion = 0;
    }

    if (you.redraw_gold)
    {
        gotoxy(xcol + 6, 10);
        cprintf( "%-8d", you.gold );
        you.redraw_gold = 0;
    }

    if (you.redraw_experience)
    {
        gotoxy(xcol + 12, 11);

#if DEBUG_DIAGNOSTICS
        cprintf( "%d/%lu  (%d/%d)",
                 you.experience_level, you.experience,
                 you.skill_cost_level, you.exp_available );
#else
        cprintf( "%d/%lu  (%d)",
                 you.experience_level, you.experience, you.exp_available );
#endif

        clear_to_end_of_line();
        you.redraw_experience = 0;
    }

    if (you.wield_change)
    {
        gotoxy(xcol, 13);
        clear_to_end_of_line();
        gotoxy(xcol, 13);

        if (you.equip[EQ_WEAPON] != -1)
        {

            const item_def& wpn = you.inv[you.equip[EQ_WEAPON]];
            textcolor(wpn.colour);

            const std::string prefix = menu_colour_item_prefix(wpn);
            const int prefcol = menu_colour(wpn.name(DESC_INVENTORY), prefix);
            if (prefcol != -1)
                textcolor(prefcol);

            cprintf("%s",
                    wpn.name(DESC_INVENTORY,
                             Options.terse_hand).substr(0,38).c_str());
            textcolor(LIGHTGREY);
        }
        else
        {
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
        gotoxy(xcol, 14);
        clear_to_end_of_line();
        gotoxy(xcol, 14);

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

        case HS_VERY_FULL:
            textcolor( GREEN );
            cprintf( "Very Full" );
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

        case HS_VERY_HUNGRY:
            textcolor( YELLOW );
            cprintf( "Very Hungry" );
            break;

        case HS_NEAR_STARVING:
            textcolor( YELLOW );
            cprintf( "Near Starving" );
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
        gotoxy(xcol, 15);
        clear_to_end_of_line();
        gotoxy(xcol, 15);
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
                dur_colour( you.light_flight()? BLUE : MAGENTA,
                            (you.duration[DUR_LEVITATION] <= 10 && !perm) );
                cprintf( "Fly " );
            }
            else
            {
                dur_colour( BLUE, (you.duration[DUR_LEVITATION] <= 10 && !perm) );
                cprintf( "Lev " );
            }
        }

        if (you.duration[DUR_INVIS])
        {
            dur_colour( BLUE, (you.duration[DUR_INVIS] <= 6) );
            cprintf( "Invis " );
        }

        if (you.duration[DUR_SILENCE])
        {
            dur_colour( BLUE, (you.duration[DUR_SILENCE] <= 5) );
            cprintf( "Sil " );
        }

        // Perhaps this should be reversed to show when it can be used?
        // In that case, it should be probably be GREEN, and we'd have
        // to check to see if the player does have a breath weapon. -- bwr
        if (you.duration[DUR_BREATH_WEAPON] &&
            wherex() < get_number_of_cols() - 4)
        {
            textcolor( YELLOW );  // no warning
            cprintf( "BWpn" );
        }
        
        if (you.duration[DUR_BARGAIN] && wherex() < get_number_of_cols() - 4)
        {
            dur_colour( BLUE, (you.duration[DUR_BARGAIN] <= 15) );
            cprintf( "Brgn" );
        }

        textcolor( LIGHTGREY );
    }

    if (you.redraw_status_flags & REDRAW_LINE_3_MASK)
    {
        gotoxy(xcol, 16);
        clear_to_end_of_line();
        gotoxy(xcol, 16);
        // Max length of this line = 7 * 5 + 3 - 1 = 37

        // Note the usage of bad_ench_colour() correspond to levels that
        // can be found in player.cc, ie those that the player can tell by
        // using the '@' command.  Things like confusion and sticky flame
        // hide their amounts and are thus always the same colour (so
        // we're not really exposing any new information). --bwr
        if (you.duration[DUR_CONF])
        {
            textcolor( RED );   // no different levels
            cprintf( "Conf " );
        }

        if (you.duration[DUR_LIQUID_FLAMES])
        {
            textcolor( RED );   // no different levels
            cprintf( "Fire " );
        }

        if (you.duration[DUR_POISONING])
        {
            // We skip marking "quite" poisoned and instead mark the
            // levels where the rules for dealing poison damage change
            // significantly.  See acr.cc for that code. -- bwr
            textcolor( bad_ench_colour( you.duration[DUR_POISONING], 5, 10 ) );
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

        if (you.attribute[ATTR_HELD])
        {
            textcolor( RED );
            cprintf( "Held " );
        }

        if (you.backlit())
        {
            textcolor(
                you.magic_contamination >= 5?
                bad_ench_colour( you.magic_contamination, 15, 25 )
                : LIGHTBLUE );
            cprintf( "Glow " );
        }

        if (you.duration[DUR_SWIFTNESS])
        {
            dur_colour( BLUE, (you.duration[DUR_SWIFTNESS] <= 6) );
            cprintf( "Swift " );
        }

        if (you.duration[DUR_SLOW] && !you.duration[DUR_HASTE])
        {
            textcolor( RED );  // no end of effect warning
            cprintf( "Slow" );
        }
        else if (you.duration[DUR_HASTE] && !you.duration[DUR_SLOW])
        {
            dur_colour( BLUE, (you.duration[DUR_HASTE] <= 6) );
            cprintf( "Fast" );
        }

        textcolor( LIGHTGREY );
    }

    you.redraw_status_flags = 0;

#if DEBUG_DIAGNOSTICS
    // debug mode GPS
    gotoxy(xcol, 17);
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
    if (equip == EQ_RINGS || equip == EQ_LEFT_RING || equip == EQ_RIGHT_RING)
        return "Ring";

    if (equip == EQ_BOOTS &&
           (you.species == SP_CENTAUR || you.species == SP_NAGA))
        return "Barding";

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

std::vector<formatted_string> get_full_detail(bool calc_unid, long sc)
{
    char buf[1000];
    // 3 columns, splits at columns 32, 52
    column_composer cols(3, 33, 52);
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

    const std::string score =
        (sc > -1? make_stringf(" (%ld points)", sc) : "");
    
    snprintf(buf, sizeof buf,
             "<yellow>%s the %s</yellow>%s\n\n"
             "Race       : %s\n"
             "Class      : %s\n"
             "Worship    : %s%s\n"
             "Level      : %7d\n"
             "Exp        : %7lu\n",
             you.your_name, player_title(),
             score.c_str(),
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
        snprintf(buf, sizeof buf,
                 "Play time  : %10s\n"
                 "Turns      : %10ld\n",
                 make_time_string(curr).c_str(), you.num_turns );
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
        const int e_order[] =
        {
            EQ_WEAPON, EQ_BODY_ARMOUR, EQ_SHIELD, EQ_HELMET, EQ_CLOAK,
            EQ_GLOVES, EQ_BOOTS, EQ_AMULET, EQ_RIGHT_RING, EQ_LEFT_RING
        };

        for(int i = 0; i < NUM_EQUIP; i++)
        {
            int eqslot = e_order[i];
            const char *slot = equip_slot_to_name( eqslot );

            if ( you.equip[ e_order[i] ] != -1)
            {
                const item_def& item = you.inv[you.equip[e_order[i]]];
                const char* colname = colour_to_str(item.colour);
                snprintf(buf, sizeof buf, "%-7s: <%s>%s</%s>",
                         slot, colname,
                         item.name(DESC_PLAIN).substr(0,38).c_str(), colname);
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
        const int rrtel = !!player_teleport(calc_unid);
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

static std::string status_mut_abilities(void);

// new scrollable status overview screen,
// including stats, mutations etc.
void print_overview_screen()
{
    bool calc_unid = false;
    formatted_scroller cmd_help;
    // Set flags, and don't use easy exit.
    cmd_help.set_flags(MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP, false);
    cmd_help.set_more( formatted_string::parse_string(
                       "<cyan>[ + : Page down.   - : Page up.   Esc exits.]"));

    std::string text;
    char title[50];
    snprintf(title, sizeof title, " the %s ", player_title());

    char race_class[50];
    snprintf(race_class, sizeof race_class,
             "(%s %s)",
             species_name(you.species, you.experience_level), you.class_name);

    char time_turns[50] = "";

    if (you.real_time != -1)
    {
        const time_t curr = you.real_time + (time(NULL) - you.start_time);
        snprintf(time_turns, sizeof time_turns,
                 " Turns: %ld, Time: %s",
                 you.num_turns, make_time_string(curr, true).c_str() );
    }

    int linelength = strlen(you.your_name) + strlen(title)
                     + strlen(race_class) + strlen(time_turns);
    for (int count = 0; linelength >= get_number_of_cols() && count < 2; count++ )
    {
        switch (count)
        {
          case 0:
              snprintf(race_class, sizeof race_class,
                       "(%s%s)",
                       get_species_abbrev(you.species),
                       get_class_abbrev(you.char_class) );
              break;
          case 1:
              strcpy(title, "");
              break;
          default:
              break;
        }
        linelength = strlen(you.your_name) + strlen(title)
            + strlen(race_class) + strlen(time_turns);
    }

    text = "<yellow>";
    text += you.your_name;
    text += title;
    text += race_class;
    text += std::string(get_number_of_cols() - linelength - 1, ' ');
    text += time_turns;
    text += "</yellow>\n";
    
    cmd_help.add_text(text);
    
    char buf[1000];
    // 4 columns
    column_composer cols1(4, 18, 30, 40);

    if (!player_rotted())
        snprintf(buf, sizeof buf, "HP %3d/%d",you.hp,you.hp_max);
    else
        snprintf(buf, sizeof buf, "HP %3d/%d (%d)",
                 you.hp, you.hp_max, you.hp_max + player_rotted() );

    cols1.add_formatted(0, buf, false);

    snprintf(buf, sizeof buf, "MP %3d/%d",
             you.magic_points, you.max_magic_points);

    cols1.add_formatted(0, buf, false);

    if (you.strength == you.max_strength)
        snprintf(buf, sizeof buf, "Str %2d", you.strength);
    else
        snprintf(buf, sizeof buf, "Str <yellow>%2d</yellow> (%d)",
                 you.strength, you.max_strength);
    cols1.add_formatted(1, buf, false);

    if (you.intel == you.max_intel)
        snprintf(buf, sizeof buf, "Int %2d", you.intel);
    else
        snprintf(buf, sizeof buf, "Int <yellow>%2d</yellow> (%d)",
                 you.intel, you.max_intel);
    cols1.add_formatted(1, buf, false);

    if (you.dex == you.max_dex)
        snprintf(buf, sizeof buf, "Dex %2d", you.dex);
    else
        snprintf(buf, sizeof buf, "Dex <yellow>%2d</yellow> (%d)",
                 you.dex, you.max_dex);
    cols1.add_formatted(1, buf, false);

    snprintf(buf, sizeof buf,
             "AC %2d\n"
             "EV %2d\n"
             "Sh %2d\n",
             player_AC(),
             player_evasion(),
             player_shield_class());
    cols1.add_formatted(2, buf, false);

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

    int xp_needed = (exp_needed(you.experience_level + 2) - you.experience) + 1;
    snprintf(buf, sizeof buf,
             "Exp: %d/%lu (%d)%s\n"
             "God: %s%s<lightgrey>      Gold: %d\n"
             "Spells: %2d memorised, %2d level%s left\n",
             you.experience_level, you.experience, you.exp_available,
             (you.experience_level < 27?
              make_stringf(", need: %d", xp_needed).c_str() : ""),
             god_colour_tag, godpowers.c_str(), you.gold,
             you.spell_no, player_spell_levels(), (player_spell_levels() == 1) ? "" : "s");
    cols1.add_formatted(3, buf, false);

    std::vector<formatted_string> blines = cols1.formatted_lines();
    unsigned i;
    for (i = 0; i < blines.size(); ++i )
        cmd_help.add_item_formatted_string(blines[i]);
    cmd_help.add_text(" ");

    // 3 columns, splits at columns 21, 38
    column_composer cols(3, 21, 38);

    const int rfire = player_res_fire(calc_unid);
    const int rcold = player_res_cold(calc_unid);
    const int rlife = player_prot_life(calc_unid);
    const int rpois = player_res_poison(calc_unid);
    const int relec = player_res_electricity(calc_unid);
    const int rsust = player_sust_abil(calc_unid);
    const int rmuta = wearing_amulet(AMU_RESIST_MUTATION, calc_unid);
    const int rslow = wearing_amulet(AMU_RESIST_SLOW, calc_unid);

    snprintf(buf, sizeof buf,
             "%sRes.Fire  : %s\n"
             "%sRes.Cold  : %s\n"
             "%sLife Prot.: %s\n"
             "%sRes.Poison: %s\n"
             "%sRes.Elec. : %s\n"
             "\n"
             "%sSust.Abil.: %s\n"
             "%sRes.Mut.  : %s\n"
             "%sRes.Slow  : %s\n",
             determine_color_string(rfire), itosym3(rfire),
             determine_color_string(rcold), itosym3(rcold),
             determine_color_string(rlife), itosym3(rlife),
             determine_color_string(rpois), itosym1(rpois),
             determine_color_string(relec), itosym1(relec),
             determine_color_string(rsust), itosym1(rsust),
             determine_color_string(rmuta), itosym1(rmuta),
             determine_color_string(rslow), itosym1(rslow));
    cols.add_formatted(0, buf, false);

    int saplevel = 0;
    switch (you.species)
    {
      case SP_GHOUL:
          saplevel = 3;
          snprintf(buf, sizeof buf, "%sSaprovore : %s",
                   determine_color_string(3), itosym3(3) );
          break;

      case SP_KOBOLD:
      case SP_TROLL:
          saplevel = 2;
          snprintf(buf, sizeof buf, "%sSaprovore : %s",
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
        pregourmand = "Gourmand  : ";
        postgourmand = itosym1(1);
        saplevel = 1;
    }
    else
    {
        pregourmand = "Saprovore : ";
        postgourmand = itosym3(saplevel);
    }
    snprintf(buf, sizeof buf, "%s%s%s",
             determine_color_string(saplevel), pregourmand, postgourmand);
    cols.add_formatted(0, buf, false);


    const int rinvi = player_see_invis(calc_unid);
    const int rward = wearing_amulet(AMU_WARDING, calc_unid) ||
        (you.religion == GOD_VEHUMET && you.duration[DUR_PRAYER] &&
         !player_under_penance() && you.piety >= piety_breakpoint(2));
    const int rcons = wearing_amulet(AMU_CONSERVATION, calc_unid);
    const int rcorr = wearing_amulet(AMU_RESIST_CORROSION, calc_unid);
    const int rclar = wearing_amulet(AMU_CLARITY, calc_unid);
    snprintf(buf, sizeof buf,
             "%sSee Invis. : %s\n"
             "%sWarding    : %s\n"
             "%sConserve   : %s\n"
             "%sRes.Corr.  : %s\n"
             "%sClarity    : %s\n"
             "\n",
             determine_color_string(rinvi), itosym1(rinvi),
             determine_color_string(rward), itosym1(rward),
             determine_color_string(rcons), itosym1(rcons),
             determine_color_string(rcorr), itosym1(rcorr),
             determine_color_string(rclar), itosym1(rclar));
    cols.add_formatted(1, buf, false);

    if ( scan_randarts(RAP_PREVENT_TELEPORTATION, calc_unid) )
        snprintf(buf, sizeof buf, "\n%sPrev.Telep.: %s",
                 determine_color_string(-1), itosym1(1));
    else
    {
        const int rrtel = !!player_teleport(calc_unid);
        snprintf(buf, sizeof buf, "\n%sRnd.Telep. : %s",
                 determine_color_string(rrtel), itosym1(rrtel));
    }
    cols.add_formatted(1, buf, false);

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
    cols.add_formatted(1, buf, false);

    {
        const int e_order[] =
        {
            EQ_WEAPON, EQ_BODY_ARMOUR, EQ_SHIELD, EQ_HELMET, EQ_CLOAK,
            EQ_GLOVES, EQ_BOOTS, EQ_AMULET, EQ_RIGHT_RING, EQ_LEFT_RING
        };

        for(i = 0; i < NUM_EQUIP; i++)
        {
            int eqslot = e_order[i];
            const char *slot = equip_slot_to_name( eqslot );

            if ( you.equip[ e_order[i] ] != -1)
            {
                const item_def& item = you.inv[you.equip[e_order[i]]];
                const char* colname = colour_to_str(item.colour);
                snprintf(buf, sizeof buf, "%-7s: <%s>%s</%s>",
                         slot, colname,
                         item.name(DESC_PLAIN).substr(0,37).c_str(), colname);
            }
            else
            {
                if (e_order[i] == EQ_WEAPON)
                {
                    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
                        snprintf(buf, sizeof buf, "%-7s: Blade Hands", slot);
                    else if (you.skills[SK_UNARMED_COMBAT])
                        snprintf(buf, sizeof buf, "%-7s: Unarmed", slot);
                    else if (!you_tran_can_wear(EQ_WEAPON))
                    {
                        snprintf(buf, sizeof buf, "%-7s: "
                                 "<darkgray>(currently unavailable)</darkgray>",
                                 slot);
                    }
                    else
                        snprintf(buf, sizeof buf, "%-7s:", slot);
                }
                else if (!you_can_wear(e_order[i], true))
                {
                    snprintf(buf, sizeof buf,
                             "%-7s: <darkgray>(unavailable)</darkgray>", slot);
                }
                else if (!you_tran_can_wear(e_order[i], true))
                {
                    snprintf(buf, sizeof buf, "%-7s: "
                             "<darkgray>(currently unavailable)</darkgray>",
                             slot);
                }
                else if (!you_can_wear(e_order[i]))
                {
                    snprintf(buf, sizeof buf,
                             "%-7s: <lightgray>(restricted)</lightgray>",
                             slot);
                }
                else
                {
                    snprintf(buf, sizeof buf, "%-7s:", slot);
                }
            }
            cols.add_formatted(2, buf, false);
        }
    }

    blines = cols.formatted_lines();

    for (i = 0; i < blines.size(); ++i )
        cmd_help.add_item_formatted_string(blines[i]);
    cmd_help.add_text(" ");

    cmd_help.add_text(status_mut_abilities());
    cmd_help.show();
    redraw_screen();
}

// creates rows of short descriptions for current
// status, mutations and abilities
std::string status_mut_abilities()
{
    //----------------------------
    // print status information
    //----------------------------
    std::string text = "<w>@:</w> ";

    if (you.burden_state == BS_ENCUMBERED)
        text += "burdened, ";
    else if (you.burden_state == BS_OVERLOADED)
        text += "overloaded, ";

    if (you.duration[DUR_BREATH_WEAPON])
        text += "short of breath, ";

    if (you.duration[DUR_REPEL_UNDEAD])
        text += "repel undead, ";

    if (you.duration[DUR_LIQUID_FLAMES])
        text += "liquid flames, ";

    if (you.duration[DUR_ICY_ARMOUR])
        text += "icy shield, ";

    if (you.duration[DUR_DEFLECT_MISSILES])
        text += "deflect missiles, ";
    else if (you.duration[DUR_REPEL_MISSILES])
        text += "repel missiles, ";

    if (you.duration[DUR_PRAYER])
        text += "praying, ";

    if (you.duration[DUR_REGENERATION]
        && (you.species != SP_VAMPIRE || you.hunger_state >= HS_HUNGRY)
        || you.species == SP_VAMPIRE && you.hunger_state >= HS_FULL)
    {
        if (you.disease)
            text += "recuperating, ";
        else
            text += "regenerating, ";
    }

// not used as resistance part already says so
//    if (you.duration[DUR_INSULATION])
//        text += "insulated, ";

    if (you.duration[DUR_STONEMAIL])
        text += "stone mail, ";

    if (you.duration[DUR_STONESKIN])
        text += "stone skin, ";

// resistance part already says so
//    if (you.duration[DUR_CONTROLLED_FLIGHT])
//        text += "control flight, ";

    if (you.duration[DUR_TELEPORT])
        text += "about to teleport, ";

// resistance part already says so
//    if (you.duration[DUR_CONTROL_TELEPORT])
//        text += "control teleport, ";

    if (you.duration[DUR_DEATH_CHANNEL])
        text += "death channel, ";

    if (you.duration[DUR_FORESCRY]) 
        text += "forewarned, ";

    if (you.duration[DUR_SILENCE])  
        text += "radiating silence, ";

    if (you.duration[DUR_INVIS])
        text += "invisible, ";

    if (you.duration[DUR_CONF])
        text += "confused, ";

    if (you.duration[DUR_PARALYSIS])
        text += "paralysed, ";

    if (you.duration[DUR_EXHAUSTED])
        text += "exhausted, ";

    if (you.duration[DUR_MIGHT])
        text += "mighty, ";

    if (you.duration[DUR_BERSERKER])
        text += "berserking, ";

    if (player_is_levitating())
        text += "levitating, ";

    if (you.duration[DUR_BARGAIN])
        text += "charismatic, ";

    if (you.duration[DUR_SLAYING])
        text += "deadly, ";

    // DUR_STEALTHY handled in stealth printout

    if (you.duration[DUR_MAGIC_SHIELD])
        text += "shielded, ";

    if (you.duration[DUR_POISONING])
    {
        text +=   (you.duration[DUR_POISONING] > 10) ? "extremely" :
                  (you.duration[DUR_POISONING] > 5)  ? "very" :
                  (you.duration[DUR_POISONING] > 3)  ? "quite"
                                       : "mildly";
        text += " poisoned, ";
    }

    if (you.disease)
    {
        text +=   (you.disease > 120) ? "badly " :
                  (you.disease >  40) ? ""
                                      : "mildly ";
        text += "diseased, ";
    }

    if (you.rotting || you.species == SP_GHOUL)
        text += "rotting, ";

    if (you.duration[DUR_CONFUSING_TOUCH])
        text += "confusing touch, ";

    if (you.duration[DUR_SURE_BLADE])
        text += "bonded with blade, ";

    int move_cost = (player_speed() * player_movement_speed()) / 10;
    if ( you.duration[DUR_SLOW] )
        move_cost *= 2;

    text +=   (move_cost <   8) ? "very quick, " :
              (move_cost <  10) ? "quick, " :
              (move_cost == 10) ? "" :
              (move_cost <  13) ? "slow, " : "";

    if (you.duration[DUR_SLOW] && !you.duration[DUR_HASTE])
        text += "slowed, ";
    else if (you.duration[DUR_HASTE] && !you.duration[DUR_SLOW])
        text += "hasted, ";
    else if (!you.duration[DUR_HASTE] && you.duration[DUR_SWIFTNESS])
        text += "swift, ";

    if (you.disease
        || you.species == SP_VAMPIRE && you.hunger_state < HS_HUNGRY)
    {
        text += "non-regenerating, ";
    }

    if (you.attribute[ATTR_HELD])
        text += "held, ";

    const int mr = player_res_magic();
    snprintf(info, INFO_SIZE, "%s resistant to magic, ",
             (mr <  10) ? "not" :
             (mr <  30) ? "slightly" :
             (mr <  60) ? "somewhat" :
             (mr <  90) ? "quite" :
             (mr < 120) ? "very" :
             (mr < 140) ? "extremely" :
             "incredibly");
             
    text += info;

    // character evaluates their ability to sneak around:
    const int ustealth = check_stealth();

    snprintf( info, INFO_SIZE, "%sstealthy",
          (ustealth <  10) ? "extremely un" :
          (ustealth <  20) ? "very un" :
          (ustealth <  30) ? "un" :
          (ustealth <  50) ? "fairly " :
          (ustealth <  80) ? "" :
          (ustealth < 120) ? "quite " :
          (ustealth < 160) ? "very " :
          (ustealth < 200) ? "extremely "
          : "incredibly " );

    text += info;

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
      case TRAN_SPIDER:
          text += "\nYou are in spider-form.";
          break;
     case TRAN_BAT:
          text += "\nYou are in ";
          if (you.species == SP_VAMPIRE)
              text += "vampire ";
          text += "bat-form.";
          break;
     case TRAN_BLADE_HANDS:
          text += "\nYou have blades for hands.";
          break;
      case TRAN_STATUE:
          text += "\nYou are a statue.";
          break;
      case TRAN_ICE_BEAST:
          text += "\nYou are an ice creature.";
          break;
      case TRAN_DRAGON:
          text += "\nYou are in dragon-form.";
          break;
      case TRAN_LICH:
          text += "\nYou are in lich-form.";
          break;
      case TRAN_SERPENT_OF_HELL:
          text += "\nYou are a huge demonic serpent.";
          break;
      case TRAN_AIR:
          text += "\nYou are a cloud of diffuse gas.";
          break;
    }

    const int to_hit = calc_your_to_hit( false ) * 2;

    snprintf( info, INFO_SIZE,
          "\n%s in your current equipment.",
          (to_hit <   1) ? "You are completely incapable of fighting" :
          (to_hit <   5) ? "Hitting even clumsy monsters is extremely awkward" :
          (to_hit <  10) ? "Hitting average monsters is awkward" :
          (to_hit <  15) ? "Hitting average monsters is difficult" :
          (to_hit <  20) ? "Hitting average monsters is hard" :
          (to_hit <  30) ? "Very agile monsters are a bit awkward to hit" :
          (to_hit <  45) ? "Very agile monsters are a bit difficult to hit" :
          (to_hit <  60) ? "Very agile monsters are a bit hard to hit" :
          (to_hit < 100) ? "You feel comfortable with your ability to fight"
                         : "You feel confident with your ability to fight" );
    text += info;

    if (you.duration[DUR_DEATHS_DOOR])
        text += "\nYou are standing in death's doorway.";

    //----------------------------
    // print mutation information
    //----------------------------
    text += "\n<w>A:</w> ";

    bool have_any = false;
    int AC_change = 0;
    int EV_change = 0;
    int Str_change = 0;
    int Int_change = 0;
    int Dex_change = 0;

    switch (you.species)   //mv: following code shows innate abilities - if any
    {
      case SP_MERFOLK:
          text += "change form in water";
          have_any = true;
          break;

      case SP_NAGA:
          // breathe poison replaces spit poison:
          if (!you.mutation[MUT_BREATHE_POISON])
              text += "spit poison";
          else
              text += "breathe poison";

          have_any = true;
          break;

      case SP_TROLL:
          text += "saprovore 2";
          have_any = true;
          break;

      case SP_GHOUL:
          text += "saprovore 3";
          have_any = true;
          break;

      case SP_GREY_ELF:
          if (you.experience_level > 4)
          {
              text += "charming";
              have_any = true;
          }
          break;

      case SP_HIGH_ELF:
          if (you.experience_level > 14)
          {
              text += "charming";
              have_any = true;
          }
          break;

      case SP_KENKU:
          text += "cannot wear helmets";
          if (you.experience_level > 4)
          {
              text += ", able to fly";
              if (you.experience_level > 14)
                  text += " continuously";
          }
          have_any = true;
          break;

      case SP_MUMMY:
          text += "in touch with death";

          have_any = true;
          break;

      case SP_GREY_DRACONIAN:
          if (you.experience_level > 6)
          {
              text += "spiky tail";
              have_any = true;
          }
          break;

      case SP_GREEN_DRACONIAN:
          if (you.experience_level > 6)
          {
              text += "breathe poison";
              have_any = true;
          }
          break;

      case SP_RED_DRACONIAN:
          if (you.experience_level > 6)
          {
              text += "breathe fire";
              have_any = true;
          }
          break;

      case SP_WHITE_DRACONIAN:
          if (you.experience_level > 6)
          {
              text += "breathe frost";
              have_any = true;
          }
          break;

      case SP_BLACK_DRACONIAN:
          if (you.experience_level > 6)
          {
              text += "breathe lightning";
              have_any = true;
          }
          break;

      case SP_GOLDEN_DRACONIAN:
          if (you.experience_level > 6)
          {
              text += "spit acid";
              text += ", acid resistance";
              have_any = true;
          }
          break;

      case SP_PURPLE_DRACONIAN:
          if (you.experience_level > 6)
          {
              text += "breathe power";
              have_any = true;
          }
          break;

      case SP_MOTTLED_DRACONIAN:
          if (you.experience_level > 6)
          {
              text += "breathe sticky flames";
              have_any = true;
          }
          break;

      case SP_PALE_DRACONIAN:
          if (you.experience_level > 6)
          {
              text += "breathe steam";
              have_any = true;
          }
          break;

      case SP_KOBOLD:
          text += "saprovore 2";
          have_any = true;
          break;

      case SP_HILL_ORC:
      case SP_OGRE:
          text += "saprovore 1";
          have_any = true;
          break;
      default:
          break;
    }                           //end switch - innate abilities

    // a bit more stuff
    if ( (you.species >= SP_OGRE && you.species <= SP_OGRE_MAGE) ||
         player_genus(GENPC_DRACONIAN) ||
         you.species == SP_SPRIGGAN )
    {
        if (have_any)
            text += ", ";
        text += "unfitting armour";
        have_any = true;
    }

    if ( beogh_water_walk() )
    {
        if (have_any)
            text += ", ";
        text += "water walking";
        have_any = true;
    }

    for (unsigned i = 0; i < NUM_MUTATIONS; i++)
    {
        if (!you.mutation[i])
            continue;

        int level = you.mutation[ i ];

        switch(i)
        {
            case MUT_TOUGH_SKIN:
                AC_change += level;
                break;
            case MUT_STRONG:
                Str_change += level;
                break;
            case MUT_CLEVER:
                Int_change += level;
                break;
            case MUT_AGILE:
                Dex_change += level;
                break;
            case MUT_GREEN_SCALES:
                AC_change += 2*level-1;
                break;
            case MUT_BLACK_SCALES:
                AC_change += 3*level;
                Dex_change -= level;
                break;
            case MUT_GREY_SCALES:
                AC_change += level;
                break;
            case MUT_BONEY_PLATES:
                AC_change += level+1;
                Dex_change -= level;
                break;
            case MUT_REPULSION_FIELD:
                EV_change += 2*level-1;
                if (level == 3)
                {
                    if (have_any)
                        text += ", ";
                    text += "repel missiles";
                    have_any = true;
                }
                break;
            case MUT_POISON_RESISTANCE:
                if (have_any)
                    text += ", ";
                text += "poison resistance";
                have_any = true;
                break;
            case MUT_CARNIVOROUS:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "carnivore %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_HERBIVOROUS:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "herbivore %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_HEAT_RESISTANCE:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "fire resistance %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_COLD_RESISTANCE:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "cold resistance %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_SHOCK_RESISTANCE:
                if (have_any)
                    text += ", ";
                text += "electricity resistance";
                have_any = true;
                break;
            case MUT_REGENERATION:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "regeneration %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_FAST_METABOLISM:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "fast metabolism %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_SLOW_METABOLISM:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "slow metabolism %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_WEAK:
                Str_change -= level;
                break;
            case MUT_DOPEY:
                Int_change -= level;
                break;
            case MUT_CLUMSY:
                Dex_change -= level;
                break;
            case MUT_TELEPORT_CONTROL:
                if (have_any)
                    text += ", ";
                text += "teleport control";
                have_any = true;
                break;
            case MUT_TELEPORT:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "teleportitis %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_MAGIC_RESISTANCE:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "magic resistance %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_FAST:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "speed %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_ACUTE_VISION:
                if (have_any)
                    text += ", ";
                text += "see invisible";
                have_any = true;
                break;
            case MUT_DEFORMED:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "deformed body %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_TELEPORT_AT_WILL:
                snprintf(info, INFO_SIZE, "teleport at will %d", level);
                if (have_any)
                    text += ", ";
                text += info;
                have_any = true;
                break;
            case MUT_SPIT_POISON:
                if (have_any)
                    text += ", ";
                text += "spit poison";
                have_any = true;
                break;
            case MUT_MAPPING:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "sense surroundings %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_BREATHE_FLAMES:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "breathe flames %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_BLINK:
                if (have_any)
                    text += ", ";
                text += "blink";
                have_any = true;
                break;
            case MUT_HORNS:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "horns %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_STRONG_STIFF:
                Str_change += level;
                Dex_change -= level;
                break;
            case MUT_FLEXIBLE_WEAK:
                Str_change -= level;
                Dex_change += level;
                break;
            case MUT_LOST:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "forgetfulness %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_CLARITY:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "clarity %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_BERSERK:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "berserk %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_DETERIORATION:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "deterioration %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_BLURRY_VISION:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "blurry vision %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_MUTATION_RESISTANCE:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "mutation resistance %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_FRAIL:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "-%d%% hp", level*10);
                text += info;
                have_any = true;
                break;
            case MUT_ROBUST:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "+%d hp%%", level*10);
                text += info;
                have_any = true;
                break;
            case MUT_TORMENT_RESISTANCE:
                if (have_any)
                    text += ", ";
                text += "torment resistance";
                have_any = true;
                break;
            case MUT_NEGATIVE_ENERGY_RESISTANCE:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "life protection %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_SUMMON_MINOR_DEMONS:
                if (have_any)
                    text += ", ";
                text += "summon minor demons";
                have_any = true;
                break;
            case MUT_SUMMON_DEMONS:
                if (have_any)
                    text += ", ";
                text += "summon demons";
                have_any = true;
                break;
            case MUT_HURL_HELLFIRE:
                if (have_any)
                    text += ", ";
                text += "hurl hellfire";
                have_any = true;
                break;
            case MUT_CALL_TORMENT:
                if (have_any)
                    text += ", ";
                text += "call torment";
                have_any = true;
                break;
            case MUT_RAISE_DEAD:
                if (have_any)
                    text += ", ";
                text += "raise dead";
                have_any = true;
                break;
            case MUT_CONTROL_DEMONS:
                if (have_any)
                    text += ", ";
                text += "control demons";
                have_any = true;
                break;
            case MUT_PANDEMONIUM:
                if (have_any)
                    text += ", ";
                text += "portal to Pandemonium";
                have_any = true;
                break;
            case MUT_DEATH_STRENGTH:
                if (have_any)
                    text += ", ";
                text += "draw strength from death and destruction";
                have_any = true;
                break;
            case MUT_CHANNEL_HELL:
                if (have_any)
                    text += ", ";
                text += "channel magical energy from Hell";
                have_any = true;
                break;
            case MUT_DRAIN_LIFE:
                if (have_any)
                    text += ", ";
                text += "drain life";
                have_any = true;
                break;
            case MUT_THROW_FLAMES:
                if (have_any)
                    text += ", ";
                text += "throw flames of Gehenna";
                have_any = true;
                break;
            case MUT_THROW_FROST:
                if (have_any)
                    text += ", ";
                text += "throw frost of Cocytus";
                have_any = true;
                break;
            case MUT_SMITE:
                if (have_any)
                    text += ", ";
                text += "invoke powers of Tartarus";
                have_any = true;
                break;
            case MUT_CLAWS:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "claws %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_HOOVES:
                if (have_any)
                    text += ", ";
                text += "hooves";
                have_any = true;
                break;
            case MUT_FANGS:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "sharp teeth %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_BREATHE_POISON:
                if (have_any)
                    text += ", ";
                text += "breathe poison";
                have_any = true;
                break;
            case MUT_STINGER:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "stinger %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_BIG_WINGS:
                if (have_any)
                    text += ", ";
                text += "large and strong wings";
                have_any = true;
                break;
            case MUT_BLUE_MARKS:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "blue evil mark %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_GREEN_MARKS:
                if (have_any)
                    text += ", ";
                snprintf(info, INFO_SIZE, "green evil mark %d", level);
                text += info;
                have_any = true;
                break;
            case MUT_RED_SCALES:
                AC_change += level;
                if (level == 3)
                    AC_change++;
                break;
            case MUT_NACREOUS_SCALES:
                AC_change += 2*level-1;
                break;
            case MUT_GREY2_SCALES:
                AC_change += 2*level;
                Dex_change -= 1;
                if (level == 3)
                    Dex_change--;
                break;
            case MUT_METALLIC_SCALES:
                AC_change += 3*level+1;
                if (level == 1)
                    AC_change--;
                Dex_change -= level + 1;
                break;
            case MUT_BLACK2_SCALES:
                AC_change += 2*level-1;
                break;
            case MUT_WHITE_SCALES:
                AC_change += 2*level-1;
                break;
            case MUT_YELLOW_SCALES:
                AC_change += 2*level;
                Dex_change -= level-1;
                break;
            case MUT_BROWN_SCALES:
                AC_change += 2*level;
                if (level == 3)
                    AC_change--;
                break;
            case MUT_BLUE_SCALES:
                AC_change += level;
                break;
            case MUT_PURPLE_SCALES:
                AC_change += 2*level;
                break;
            case MUT_SPECKLED_SCALES:
                AC_change += level;
                break;
            case MUT_ORANGE_SCALES:
                AC_change += level;
                if (level > 1)
                    AC_change++;
                break;
            case MUT_INDIGO_SCALES:
                AC_change += 2*level-1;
                if (level == 1)
                    AC_change++;
                break;
            case MUT_RED2_SCALES:
                AC_change += 2*level;
                if (level > 1)
                    AC_change++;
                Dex_change -= level - 1;
                break;
            case MUT_IRIDESCENT_SCALES:
                AC_change += level;
                break;
            case MUT_PATTERNED_SCALES:
                AC_change += level;
                break;
            default: break;
        }
    }

    if (AC_change)
    {
        if (have_any)
            text += ", ";
        snprintf(info, INFO_SIZE, "AC %s%d", (AC_change > 0 ? "+" : ""), AC_change);
        text += info;
        have_any = true;
    }
    if (EV_change)
    {
        if (have_any)
            text += ", ";
        snprintf(info, INFO_SIZE, "EV +%d", EV_change);
        text += info;
        have_any = true;
    }
    if (Str_change)
    {
        if (have_any)
            text += ", ";
        snprintf(info, INFO_SIZE, "Str %s%d", (Str_change > 0 ? "+" : ""), Str_change);
        text += info;
        have_any = true;
    }
    if (Int_change)
    {
        if (have_any)
            text += ", ";
        snprintf(info, INFO_SIZE, "Int %s%d", (Int_change > 0 ? "+" : ""), Int_change);
        text += info;
        have_any = true;
    }
    if (Dex_change)
    {
        if (have_any)
            text += ", ";
        snprintf(info, INFO_SIZE, "Dex %s%d", (Dex_change > 0 ? "+" : ""), Dex_change);
        text += info;
        have_any = true;
    }

    if (!have_any)
        text +=  "no striking features";

    //----------------------------
    // print ability information
    //----------------------------
    
    text += print_abilities();
    linebreak_string2(text, get_number_of_cols());

    return text;
}

