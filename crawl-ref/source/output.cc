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
#include <sstream>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "branch.h"
#include "describe.h"
#include "directn.h"
#include "format.h"
#include "fight.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "newgame.h"
#include "ouch.h"
#include "player.h"
#include "place.h"
#include "quiver.h"
#include "religion.h"
#include "skills2.h"
#include "stuff.h"
#include "transfor.h"
#ifdef USE_TILE
 #include "tiles.h"
#endif
#include "travel.h"
#include "view.h"

// Color for captions like 'Health:', 'Str:'
#define HUD_CAPTION_COLOR Options.status_caption_colour

// Color for values, which come after captions
static const short HUD_VALUE_COLOR = LIGHTGREY;

// ----------------------------------------------------------------------
// colour_bar
// ----------------------------------------------------------------------

class colour_bar
{
    typedef unsigned short color_t;
 public:
    colour_bar(color_t default_colour,
               color_t change_pos,
               color_t change_neg,
               color_t empty)
        : m_default(default_colour), m_change_pos(change_pos),
          m_change_neg(change_neg), m_empty(empty),
          m_old_disp(-1),
          m_request_redraw_after(0)
    {
        // m_old_disp < 0 means it's invalid and needs to be initialized
    }

    bool wants_redraw() const
    {
        return (m_request_redraw_after && you.num_turns >= m_request_redraw_after);
    }

    void draw(int ox, int oy, int val, int max_val)
    {
        ASSERT(val <= max_val);
        if (max_val <= 0)
        {
            m_old_disp = -1;
            return;
        }

#ifdef USE_TILE
        // Don't redraw colour bars while resting
        // *unless* we'll stop doing so right after that
        if (you.running >= 2 && is_resting() && val != max_val)
        {
            m_old_disp = -1;
            return;
        }
#endif

        const int width = crawl_view.hudsz.x - (ox-1);
        const int disp = width * val / max_val;
        const int old_disp = (m_old_disp < 0) ? disp : m_old_disp;
        m_old_disp = disp;

        cgotoxy(ox, oy, GOTO_STAT);

        textcolor(BLACK);
        for (int cx = 0; cx < width; cx++)
        {
#ifdef USE_TILE
            // maybe this should use textbackground too?
            textcolor(BLACK + m_empty * 16);

            if (cx < disp)
                textcolor(BLACK + m_default * 16);
            else if (old_disp > disp && cx < old_disp)
                textcolor(BLACK + m_change_neg * 16);
            putch(' ');
#else
            if (cx < disp && cx < old_disp)
            {
                textcolor(m_default);
                putch('=');
            }
            else if (/* old_disp <= cx && */ cx < disp)
            {
                textcolor(m_change_pos);
                putch('=');
            }
            else if (/* disp <= cx && */ cx < old_disp)
            {
                textcolor(m_change_neg);
                putch('-');
            }
            else
            {
                textcolor(m_empty);
                putch('-');
            }

            // If some change colour was rendered, redraw in a few
            // turns to clear it out.
            if (old_disp != disp)
                m_request_redraw_after = you.num_turns + 4;
            else
                m_request_redraw_after = 0;
#endif
        }

        textcolor(LIGHTGREY);
        textbackground(BLACK);
    }

 private:
    const color_t m_default;
    const color_t m_change_pos;
    const color_t m_change_neg;
    const color_t m_empty;
    int m_old_disp;
    int m_request_redraw_after; // force a redraw at this turn count
};

colour_bar HP_Bar(LIGHTGREEN, GREEN, RED, DARKGRAY);

#ifdef USE_TILE
colour_bar MP_Bar(BLUE, BLUE, LIGHTBLUE, DARKGRAY);
#else
colour_bar MP_Bar(LIGHTBLUE, BLUE, MAGENTA, DARKGRAY);
#endif

// ----------------------------------------------------------------------
// Status display
// ----------------------------------------------------------------------

static int _bad_ench_colour( int lvl, int orange, int red )
{
    if (lvl > red)
        return (RED);
    else if (lvl > orange)
        return (LIGHTRED);

    return (YELLOW);
}

static int _dur_colour( int running_out_color, bool running_out )
{
    if (running_out)
    {
        return running_out_color;
    }
    else
    {
        switch (running_out_color)
        {
        case GREEN:     return ( LIGHTGREEN );
        case BLUE:      return ( LIGHTBLUE );
        case MAGENTA:   return ( LIGHTMAGENTA );
        case LIGHTGREY: return ( WHITE );
        default: return running_out_color;
        }
    }
}

#ifdef DGL_SIMPLE_MESSAGING
void update_message_status()
{
    textcolor(LIGHTBLUE);

    cgotoxy(36, 2, GOTO_STAT);
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
    if (!Options.show_gold_turns
        || you.running > 0
        || you.running < 0 && Options.travel_delay == -1)
    {
        return;
    }

    cgotoxy(19+6, 8, GOTO_STAT);

    // Show the turn count starting from 1. You can still quit on turn 0.
    textcolor(HUD_VALUE_COLOR);
    cprintf("%ld", you.num_turns);
    textcolor(LIGHTGREY);
}

static int _count_digits(int val)
{
    if (val > 999)
        return 4;
    else if (val > 99)
        return 3;
    else if (val > 9)
        return 2;
    return 1;
}

static const char* _describe_hunger(int& color)
{
    bool vamp = (you.species == SP_VAMPIRE);

    switch (you.hunger_state)
    {
        case HS_ENGORGED:
            color = LIGHTGREEN;
            return (vamp ? "Alive" : "Engorged");
        case HS_VERY_FULL:
            color = GREEN;
            return ("Very Full");
        case HS_FULL:
            color = GREEN;
            return ("Full");
        case HS_SATIATED: // normal
            color = GREEN;
            return NULL;
        case HS_HUNGRY:
            color = YELLOW;
            return (vamp ? "Thirsty" : "Hungry");
        case HS_VERY_HUNGRY:
            color = YELLOW;
            return (vamp ? "Very Thirsty" : "Very Hungry");
        case HS_NEAR_STARVING:
            color = YELLOW;
            return (vamp ? "Near Bloodless" : "Near Starving");
        case HS_STARVING:
        default:
            color = RED;
            return (vamp ? "Bloodless" : "Starving");
    }
}

static void _print_stats_mp(int x, int y)
{
    // Calculate colour
    short mp_colour = HUD_VALUE_COLOR;
    {
        int mp_percent = (you.max_magic_points == 0
                          ? 100
                          : (you.magic_points * 100) / you.max_magic_points);
        for ( unsigned int i = 0; i < Options.mp_colour.size(); ++i )
            if ( mp_percent <= Options.mp_colour[i].first )
                mp_colour = Options.mp_colour[i].second;
    }

    cgotoxy(x+8, y, GOTO_STAT);
    textcolor(mp_colour);
    cprintf("%d", you.magic_points);
    textcolor(HUD_VALUE_COLOR);
    cprintf("/%d", you.max_magic_points );

    int col = _count_digits(you.magic_points)
              + _count_digits(you.max_magic_points) + 1;
    for (int i = 11-col; i > 0; i--)
        cprintf(" ");

    if (! Options.classic_hud)
        MP_Bar.draw(19, y, you.magic_points, you.max_magic_points);
}

static void _print_stats_hp(int x, int y)
{
    const int max_max_hp = you.hp_max + player_rotted();

    // Calculate colour
    short hp_colour = HUD_VALUE_COLOR;
    {
        const int hp_percent =
            (you.hp * 100) / (max_max_hp ? max_max_hp : you.hp);

        for ( unsigned int i = 0; i < Options.hp_colour.size(); ++i )
            if ( hp_percent <= Options.hp_colour[i].first )
                hp_colour = Options.hp_colour[i].second;
    }

    // 01234567890123456789
    // Health: xxx/yyy (zzz)
    cgotoxy(x, y, GOTO_STAT);
    textcolor(HUD_CAPTION_COLOR);
    cprintf(max_max_hp != you.hp_max ? "HP: " : "Health: ");
    textcolor(hp_colour);
    cprintf( "%d", you.hp );
    textcolor(HUD_VALUE_COLOR);
    cprintf( "/%d", you.hp_max );
    if (max_max_hp != you.hp_max)
        cprintf( " (%d)", max_max_hp );

    int col = wherex() - crawl_view.hudp.x;
    for (int i = 18-col; i > 0; i--)
        cprintf(" ");

    if (! Options.classic_hud)
        HP_Bar.draw(19, y, you.hp, you.hp_max);
}

// XXX: alters state!  Does more than just print!
static void _print_stats_str(int x, int y)
{
    if (you.strength < 0)
        you.strength = 0;
    else if (you.strength > 72)
        you.strength = 72;

    if (you.max_strength > 72)
        you.max_strength = 72;

    cgotoxy(x+5, y, GOTO_STAT);

    if (you.duration[DUR_MIGHT])
        textcolor(LIGHTBLUE);  // no end of effect warning
    else if (you.strength < you.max_strength)
        textcolor(YELLOW);
    else
        textcolor(HUD_VALUE_COLOR);

    cprintf( "%d", you.strength );

    if (you.strength != you.max_strength)
        cprintf( " (%d)  ", you.max_strength );
    else
        cprintf( "       " );

    burden_change();
}

static void _print_stats_int(int x, int y)
{
    if (you.intel < 0)
        you.intel = 0;
    else if (you.intel > 72)
        you.intel = 72;

    if (you.max_intel > 72)
        you.max_intel = 72;

    cgotoxy(x+5, y, GOTO_STAT);

    textcolor(you.intel < you.max_intel ? YELLOW : HUD_VALUE_COLOR);
    cprintf( "%d", you.intel );

    if (you.intel != you.max_intel)
        cprintf( " (%d)  ", you.max_intel );
    else
        cprintf( "       " );
}

static void _print_stats_dex(int x, int y)
{
    if (you.dex < 0)
        you.dex = 0;
    else if (you.dex > 72)
        you.dex = 72;

    if (you.max_dex > 72)
        you.max_dex = 72;

    cgotoxy(x+5, y, GOTO_STAT);

    textcolor(you.dex < you.max_dex ? YELLOW : HUD_VALUE_COLOR);
    cprintf( "%d", you.dex );

    if (you.dex != you.max_dex)
        cprintf( " (%d)  ", you.max_dex );
    else
        cprintf( "       " );
}

static void _print_stats_ac(int x, int y)
{
    // AC:
    cgotoxy(x+4, y, GOTO_STAT);
    if (you.duration[DUR_STONEMAIL])
        textcolor(_dur_colour( BLUE, (you.duration[DUR_STONEMAIL] <= 6) ));
    else if (you.duration[DUR_ICY_ARMOUR] || you.duration[DUR_STONESKIN])
        textcolor( LIGHTBLUE );
    else
        textcolor( HUD_VALUE_COLOR );
    cprintf( "%2d ", player_AC() );

    // Sh: (one line lower)
    cgotoxy(x+4, y+1, GOTO_STAT);
    if (you.duration[DUR_CONDENSATION_SHIELD] || you.duration[DUR_DIVINE_SHIELD])
        textcolor( LIGHTBLUE );
    else
        textcolor( HUD_VALUE_COLOR );
    cprintf( "%2d ", player_shield_class() );
}

static void _print_stats_ev(int x, int y)
{
    cgotoxy(x+4, y, GOTO_STAT);
    textcolor(you.duration[DUR_FORESCRY] ? LIGHTBLUE : HUD_VALUE_COLOR);
    cprintf( "%2d ", player_evasion() );
}

static void _print_stats_wp(int y)
{
    cgotoxy(1, y, GOTO_STAT);
    textcolor(Options.status_caption_colour);
    cprintf("Wp: ");
    if (you.weapon())
    {
        const item_def& wpn = *you.weapon();
        textcolor(wpn.colour);

        const std::string prefix = menu_colour_item_prefix(wpn);
        const int prefcol = menu_colour(wpn.name(DESC_INVENTORY), prefix);
        if (prefcol != -1)
            textcolor(prefcol);

        cprintf("%s",
                wpn.name(DESC_INVENTORY, true)
                .substr(0, crawl_view.hudsz.x - 4).c_str());
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
    clear_to_end_of_line();
}

static void _print_stats_qv(int y)
{
    cgotoxy(1, y, GOTO_STAT);
    textcolor(Options.status_caption_colour);
    cprintf("Qv: ");

    const item_def* item;
    int q;
    you.m_quiver->get_desired_item(&item, &q);

    ASSERT(q >= -1 && q < ENDOFPACK);
    if (q != -1)
    {
        const item_def& quiver = *item;
        textcolor(quiver.colour);

        const std::string prefix = menu_colour_item_prefix(quiver);
        const int prefcol =
            menu_colour(quiver.name(DESC_INVENTORY), prefix);
        if (prefcol != -1)
            textcolor(prefcol);

        cprintf("%s",
                quiver.name(DESC_INVENTORY, true)
                .substr(0, crawl_view.hudsz.x - 4)
                .c_str());
    }
    else if (item != NULL && is_valid_item(*item))
    {
        textcolor(item->colour);
        cprintf("-) %s", item->name(DESC_PLAIN, true).c_str());
        textcolor(RED);
        cprintf(" (empty)");
    }
    else
    {
        textcolor(LIGHTGREY);
        cprintf("Nothing quivered");
    }

    textcolor(LIGHTGREY);
    clear_to_end_of_line();
}

struct status_light
{
    status_light(int c, const char* t) : color(c), text(t) {}
    int color;
    const char* text;
};

// The colour scheme for these flags is currently:
//
// - yellow, "orange", red      for bad conditions
// - light grey, white          for god based conditions
// - green, light green         for good conditions
// - blue, light blue           for good enchantments
// - magenta, light magenta     for "better" enchantments (deflect, fly)
//
// Prints burden, hunger,
// pray, holy, teleport, regen, insulation, fly/lev, invis, silence,
//   conf. touch, bargain, sage
// confused, beheld, fire, poison, disease, rot, held, glow/halo,
//  swift, fast, slow, breath
//
// Note the usage of bad_ench_colour() correspond to levels that
// can be found in player.cc, ie those that the player can tell by
// using the '@' command.  Things like confusion and sticky flame
// hide their amounts and are thus always the same colour (so
// we're not really exposing any new information). --bwr
static void _get_status_lights(std::vector<status_light>& out)
{
#if DEBUG_DIAGNOSTICS
    {
        static char static_pos_buf[80];
        snprintf(static_pos_buf, sizeof(static_pos_buf),
                 "%2d,%2d", you.x_pos, you.y_pos );
        out.push_back(status_light(LIGHTGREY, static_pos_buf));

        static char static_hunger_buf[80];
        snprintf(static_hunger_buf, sizeof(static_hunger_buf),
                 "(%d:%d)", you.hunger - you.old_hunger, you.hunger );
        out.push_back(status_light(LIGHTGREY, static_hunger_buf));
    }
#endif

    switch (you.burden_state)
    {
    case BS_OVERLOADED:
        out.push_back(status_light(RED, "Overloaded"));
        break;

    case BS_ENCUMBERED:
        out.push_back(status_light(LIGHTRED, "Encumbered"));
        break;

    case BS_UNENCUMBERED:
        break;
    }

    {
        int hunger_color;
        const char* hunger_text = _describe_hunger(hunger_color);
        if (hunger_text)
            out.push_back(status_light(hunger_color, hunger_text));
    }

    if (you.duration[DUR_PRAYER])
    {
        out.push_back(status_light(WHITE, "Pray"));  // no end of effect warning
    }

    if (you.duration[DUR_REPEL_UNDEAD])
    {
        int color = _dur_colour( LIGHTGREY, (you.duration[DUR_REPEL_UNDEAD] <= 4) );
        out.push_back(status_light(color, "Holy"));
    }

    if (you.duration[DUR_TELEPORT])
    {
        out.push_back(status_light(LIGHTBLUE, "Tele"));
    }

    if (you.duration[DUR_DEFLECT_MISSILES])
    {
        int color = _dur_colour( MAGENTA, (you.duration[DUR_DEFLECT_MISSILES] <= 6) );
        out.push_back(status_light(color, "DMsl"));
    }
    else if (you.duration[DUR_REPEL_MISSILES])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_REPEL_MISSILES] <= 6) );
        out.push_back(status_light(color, "RMsl"));
    }

    if (you.duration[DUR_REGENERATION])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_REGENERATION] <= 6) );
        out.push_back(status_light(color, "Regen"));
    }

    if (you.duration[DUR_INSULATION])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_INSULATION] <= 6) );
        out.push_back(status_light(color, "Ins"));
    }

    if (player_is_airborne())
    {
        const bool perm = you.permanent_flight();
        if (wearing_amulet( AMU_CONTROLLED_FLIGHT ))
        {
            int color = _dur_colour( you.light_flight()? BLUE : MAGENTA,
                        (you.duration[DUR_LEVITATION] <= 10 && !perm) );
            out.push_back(status_light(color, "Fly"));
        }
        else
        {
            int color = _dur_colour(BLUE, (you.duration[DUR_LEVITATION] <= 10 && !perm));
            out.push_back(status_light(color, "Lev"));
        }
    }

    if (you.duration[DUR_INVIS])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_INVIS] <= 6) );
        out.push_back(status_light(color, "Invis"));
    }

    if (you.duration[DUR_SILENCE])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_SILENCE] <= 5) );
        out.push_back(status_light(color, "Sil"));
    }

    if (you.duration[DUR_CONFUSING_TOUCH])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_SILENCE] <= 20) );
        out.push_back(status_light(color, "Touch"));
    }

    if (you.duration[DUR_BARGAIN])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_BARGAIN] <= 15) );
        out.push_back(status_light(color, "Brgn"));
    }

    if (you.duration[DUR_SAGE])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_SAGE] <= 15) );
        out.push_back(status_light(color, "Sage"));
    }

    if (you.duration[DUR_SURE_BLADE])
    {
        out.push_back(status_light(BLUE, "Blade"));
    }

    if (you.duration[DUR_CONF])
    {
        out.push_back(status_light(RED, "Conf"));
    }

    if (you.duration[DUR_BEHELD])
    {
        out.push_back(status_light(RED, "Bhld"));
    }

    if (you.duration[DUR_LIQUID_FLAMES])
    {
        out.push_back(status_light(RED, "Fire"));
    }

    if (you.duration[DUR_POISONING])
    {
        int color = _bad_ench_colour( you.duration[DUR_POISONING], 5, 10 );
        out.push_back(status_light(color, "Pois"));
    }

    if (you.disease)
    {
        int color = _bad_ench_colour( you.disease, 40, 120 );
        out.push_back(status_light(color, "Sick"));
    }

    if (you.rotting)
    {
        int color = _bad_ench_colour( you.rotting, 4, 8 );
        out.push_back(status_light(color, "Rot"));
    }

    if (you.attribute[ATTR_HELD])
    {
        out.push_back(status_light(RED, "Held"));
    }

    if (you.backlit())
    {
        if (!you.backlit(false) && you.haloed())
            out.push_back(status_light(LIGHTBLUE, "Halo"));
        else
        {
            int color = you.magic_contamination > 5
                ? _bad_ench_colour( you.magic_contamination, 15, 25 )
                : LIGHTBLUE;
            out.push_back(status_light(color, "Glow"));
        }
    }

    if (you.duration[DUR_SWIFTNESS])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_SWIFTNESS] <= 6) );
        out.push_back(status_light(color, "Swift"));
    }

    if (you.duration[DUR_SLOW] && !you.duration[DUR_HASTE])
    {
        out.push_back(status_light(RED, "Slow"));
    }
    else if (you.duration[DUR_HASTE] && !you.duration[DUR_SLOW])
    {
        int color = _dur_colour( BLUE, (you.duration[DUR_HASTE] <= 6) );
        out.push_back(status_light(color, "Fast"));
    }

    if (you.duration[DUR_BREATH_WEAPON])
    {
        out.push_back(status_light(YELLOW, "BWpn"));
    }

}

static void _print_status_lights(int y)
{
    std::vector<status_light> lights;
    static int last_number_of_lights = 0;
    _get_status_lights(lights);
    if (lights.size() == 0 && last_number_of_lights == 0)
        return;
    last_number_of_lights = lights.size();

    size_t line_cur = y;
    const size_t line_end = crawl_view.hudsz.y+1;

    cgotoxy(1, line_cur, GOTO_STAT);
    ASSERT(wherex()-crawl_view.hudp.x == 0);

    size_t i_light = 0;
    while (true)
    {
        const int end_x = (wherex() - crawl_view.hudp.x) +
            (i_light < lights.size() ? strlen(lights[i_light].text) : 10000);

        if (end_x <= crawl_view.hudsz.x)
        {
            textcolor(lights[i_light].color);
            cprintf("%s", lights[i_light].text);
            if (end_x < crawl_view.hudsz.x)
                cprintf(" ");
            ++ i_light;
        }
        else
        {
            clear_to_end_of_line();
            ++ line_cur;
            // Careful not to trip the )#(*$ cgotoxy ASSERT
            if (line_cur == line_end)
                break;
            cgotoxy(1, line_cur, GOTO_STAT);
        }
    }
}

void print_stats(void)
{
    textcolor(LIGHTGREY);

#ifdef USE_TILE
    if (you.redraw_armour_class || you.wield_change)
        TilePlayerRefresh();
#endif

    // Displayed evasion is now tied to dex.
    if (you.redraw_dexterity)
        you.redraw_evasion = true;

    if (HP_Bar.wants_redraw()) you.redraw_hit_points = true;
    if (MP_Bar.wants_redraw()) you.redraw_magic_points = true;

    if (you.redraw_hit_points)   { you.redraw_hit_points = false;   _print_stats_hp ( 1, 3); }
    if (you.redraw_magic_points) { you.redraw_magic_points = false; _print_stats_mp ( 1, 4); }
    if (you.redraw_strength)     { you.redraw_strength = false;     _print_stats_str( 1, 5); }
    if (you.redraw_intelligence) { you.redraw_intelligence = false; _print_stats_int( 1, 6); }
    if (you.redraw_dexterity)    { you.redraw_dexterity = false;    _print_stats_dex( 1, 7); }

    if (you.redraw_armour_class) { you.redraw_armour_class = false; _print_stats_ac (19, 5); }
    // (you.redraw_armour_class) { you.redraw_armour_class = false; _print_stats_sh (19, 6); }
    if (you.redraw_evasion)      { you.redraw_evasion = false;      _print_stats_ev (19, 7); }

    int yhack = 0;
    // If Options.show_gold_turns, line 8 is Gold and Turns
    if (Options.show_gold_turns)
    {
        yhack = 1;
        cgotoxy(1+6, 8, GOTO_STAT);
        textcolor(HUD_VALUE_COLOR);
        cprintf("%d", you.gold);
    }

    if (you.redraw_experience)
    {
        cgotoxy(1,8+yhack, GOTO_STAT);
        textcolor(Options.status_caption_colour);
        cprintf("Exp Pool: ");
        textcolor(HUD_VALUE_COLOR);
#if DEBUG_DIAGNOSTICS
        cprintf("%d/%d (%d) ",
                you.skill_cost_level, you.exp_available, you.experience);
#else
        cprintf("%-6d", you.exp_available);
#endif
        you.redraw_experience = false;
    }

    if (you.wield_change)
    {
        // weapon_change is set in a billion places; probably not all
        // of them actually mean the user changed their weapon.  Calling
        // on_weapon_changed redundantly is normally OK; but if the user
        // is wielding a bow and throwing javelins, the on_weapon_changed
        // will switch them back to arrows, which is annoying.
        // Perhaps there should be another bool besides wield_change
        // that's set in fewer places?
        // Also, it's a little bogus to change simulation state in
        // render code.  We should find a better place for this.
        you.m_quiver->on_weapon_changed();
        _print_stats_wp(9+yhack);
    }

    if (you.redraw_quiver || you.wield_change)
    {
        _print_stats_qv(10+yhack);
        you.redraw_quiver = false;
    }
    you.wield_change  = false;

    if (you.redraw_status_flags)
    {
        you.redraw_status_flags = 0;
        _print_status_lights(11+yhack);
    }
    textcolor(LIGHTGREY);

    update_screen();
}

static std::string _level_description_string_hud()
{
    const PlaceInfo& place = you.get_place_info();
    std::string short_name = place.short_name();

    if (place.level_type == LEVEL_DUNGEON
        && branches[place.branch].depth > 1)
    {
        short_name += make_stringf(":%d", player_branch_depth());
    }
    // Indefinite articles
    else if (  place.level_type == LEVEL_PORTAL_VAULT
            || place.level_type == LEVEL_LABYRINTH)
    {
        if (you.level_type_name == "bazaar")
            short_name = "A Bazaar";
        else
            short_name.insert(0, "A ");
    }
    // Definite articles
    else if (place.level_type == LEVEL_ABYSS)
    {
        short_name.insert(0, "The ");
    }
    return short_name;
}

// For some odd reason, only redrawn on level change.
void print_stats_level()
{
    int ypos = 8;
    if (Options.show_gold_turns)
        ypos++;
    cgotoxy(19, ypos, GOTO_STAT);
    textcolor(HUD_CAPTION_COLOR);
    cprintf("Level: ");

    textcolor(HUD_VALUE_COLOR);
#if DEBUG_DIAGNOSTICS
    cprintf( "(%d) ", you.your_level + 1 );
#endif
    cprintf("%s", _level_description_string_hud().c_str());
    clear_to_end_of_line();
}

void redraw_skill(const std::string &your_name, const std::string &class_name)
{
    std::string title = your_name + " the " + class_name;

    unsigned int in_len = title.length();
    const unsigned int WIDTH = crawl_view.hudsz.x;
    if (in_len > WIDTH)
    {
        in_len -= 3;  // what we're getting back from removing "the"

        const unsigned int name_len = your_name.length();
        std::string trimmed_name = your_name;

        // squeeze name if required, the "- 8" is to not squeeze too much
        if (in_len > WIDTH && (name_len - 8) > (in_len - WIDTH))
        {
            trimmed_name =
                trimmed_name.substr(0, name_len - (in_len - WIDTH) - 1);
        }

        title = trimmed_name + ", " + class_name;
    }

    // Line 1: Foo the Bar    *WIZARD*
    cgotoxy(1, 1, GOTO_STAT);
    textcolor( YELLOW );
    if (title.size() > WIDTH)
        title.resize(WIDTH, ' ');
    cprintf( "%-*s", WIDTH, title.c_str() );
    if (you.wizard)
    {
        textcolor( LIGHTBLUE );
        cgotoxy(1 + crawl_view.hudsz.x-9, 1, GOTO_STAT);
        cprintf(" *WIZARD*");
    }

    // Line 2:
    // Level N Minotaur [of God]
    textcolor( YELLOW );
    cgotoxy(1, 2, GOTO_STAT);
    cprintf("Level %d %s",
            you.experience_level,
            species_name( you.species, you.experience_level ).c_str());
    if (you.religion != GOD_NO_GOD)
        cprintf(" of %s", god_name(you.religion).c_str());

    textcolor( LIGHTGREY );
}

void draw_border(void)
{
    textcolor( HUD_CAPTION_COLOR );
    clrscr();
    redraw_skill( you.your_name, player_title() );

    textcolor(Options.status_caption_colour);

    //cgotoxy( 1, 3, GOTO_STAT); cprintf("Hp:");
    cgotoxy( 1, 4, GOTO_STAT); cprintf("Magic:");
    cgotoxy( 1, 5, GOTO_STAT); cprintf("Str:");
    cgotoxy( 1, 6, GOTO_STAT); cprintf("Int:");
    cgotoxy( 1, 7, GOTO_STAT); cprintf("Dex:");

    cgotoxy(19, 5, GOTO_STAT); cprintf("AC:");
    cgotoxy(19, 6, GOTO_STAT); cprintf("Sh:");
    cgotoxy(19, 7, GOTO_STAT); cprintf("Ev:");

    if (Options.show_gold_turns)
    {
        cgotoxy( 1, 8, GOTO_STAT); cprintf("Gold:");
        cgotoxy(19, 8, GOTO_STAT); cprintf("Turn:");
    }
    // Line 9 (or 8) is exp pool, Level
}                               // end draw_border()

// ----------------------------------------------------------------------
// Monster pane
// ----------------------------------------------------------------------

#ifndef USE_TILE

// Monster info used by the pane; precomputes some data
// to help with sorting and rendering.
class monster_pane_info
{
 public:
    static bool less_than(const monster_pane_info& m1,
                          const monster_pane_info& m2);

    monster_pane_info(const monsters* m)
        : m_mon(m)
    {
        // XXX: this doesn't take into account ENCH_NEUTRAL, but that's probably
        // a bug for mons_attitude, not this.
        // XXX: also, mons_attitude_type should be sorted hostile/neutral/friendly;
        // will break saves a little bit though.
        m_attitude = mons_attitude(m);

        // Currently, difficulty is defined as "average hp".  Leaks too much info?
        const monsterentry* me = get_monster_data(m->type);
        m_difficulty = me->hpdice[0] * (me->hpdice[1] + (me->hpdice[2]>>1))
            + me->hpdice[3];

        m_brands = 0;
        if (mons_looks_stabbable(m)) m_brands |= 1;
        if (mons_looks_distracted(m)) m_brands |= 2;
        if (m->has_ench(ENCH_BERSERK)) m_brands |= 4;
    }

    void to_string(int count, std::string& desc, int& desc_color) const;

    const monsters* m_mon;
    mon_attitude_type m_attitude;
    int m_difficulty;
    int m_brands;
};

// Sort monsters by:
//   attitude
//   difficulty
//   type
//   brand
bool // static
monster_pane_info::less_than(const monster_pane_info& m1,
                             const monster_pane_info& m2)
{
    if (m1.m_attitude < m2.m_attitude)
        return true;
    else if (m1.m_attitude > m2.m_attitude)
        return false;

    // By descending difficulty
    if (m1.m_difficulty > m2.m_difficulty)
        return true;
    else if (m1.m_difficulty < m2.m_difficulty)
        return false;

    if (m1.m_mon->type < m2.m_mon->type)
        return true;
    else if (m1.m_mon->type > m2.m_mon->type)
        return false;

#if 0 // for now, sort brands together.
    // By descending brands, so no brands sorts to the end
    if (m1.m_brands > m2.m_brands)
        return true;
    else if (m1.m_brands < m2.m_brands)
        return false;
#endif

    return false;
}

void
monster_pane_info::to_string(
    int count,
    std::string& desc,
    int& desc_color) const
{
    std::ostringstream out;

    if (count == 1)
    {
        out << mons_type_name(m_mon->type, DESC_PLAIN);
    }
    else
    {
        out << count << " "
            << pluralise(mons_type_name(m_mon->type, DESC_PLAIN));
    }

#if DEBUG_DIAGNOSTICS
    out << " av" << m_difficulty << " "
        << m_mon->hit_points << "/" << m_mon->max_hit_points;
#endif

    if (count == 1)
    {
        if (m_mon->has_ench(ENCH_BERSERK))
            out << " (berserk)";
        else if (mons_looks_stabbable(m_mon))
            out << " (resting)";
        else if (mons_looks_distracted(m_mon))
            out << " (distracted)";
        else if (m_mon->has_ench(ENCH_INVIS))
            out << " (invisible)";
    }

    // Friendliness
    switch (m_attitude)
    {
    case ATT_FRIENDLY:
        //out << " (friendly)";
        desc_color = GREEN;
        break;
    case ATT_GOOD_NEUTRAL:
    case ATT_NEUTRAL:
        //out << " (neutral)";
        desc_color = BROWN;
        break;
    case ATT_HOSTILE:
        // out << " (hostile)";
        desc_color = LIGHTGREY;
        break;
    }

    desc = out.str();
}

static void
_print_next_monster_desc(const std::vector<monster_pane_info>& mons, int& start)
{
    // skip forward to past the end of the range of identical monsters
    unsigned int end;
    for (end=start+1; end < mons.size(); ++end)
    {
        // Array is sorted, so if !(m1 < m2), m1 and m2 are "equal"
        if (monster_pane_info::less_than(mons[start], mons[end]))
            break;
    }
    // Postcondition: all monsters in [start, end) are "equal"

    // Print info on the monsters we've found
    {
        int printed = 0;

        // one glyph for each monster
        for (unsigned int i_mon=start; i_mon<end; i_mon++)
        {
            unsigned int glyph;
            unsigned short glyph_color;
            get_mons_glyph(mons[i_mon].m_mon, &glyph, &glyph_color);
            textcolor(glyph_color);
            cprintf( stringize_glyph(glyph).c_str() );
            ++ printed;
            // Printing too many looks pretty bad, though.
            if (i_mon > 6)
                break;
        }
        textcolor(LIGHTGREY);

        const int count = (end - start);

        if (count == 1)
        {
            // Print an "icon" representing damage level
            std::string damage_desc;
            mon_dam_level_type damage_level;
            mons_get_damage_level(mons[start].m_mon, damage_desc, damage_level);
            int dam_color;
            switch (damage_level)
            {
                // NOTE: in os x, light versions of foreground colors are OK,
                // but not background colors.  So stick wth standards.
            case MDAM_DEAD:
            case MDAM_ALMOST_DEAD:
            case MDAM_HORRIBLY_DAMAGED:   dam_color = RED;       break;
            case MDAM_HEAVILY_DAMAGED:    dam_color = MAGENTA;   break;
            case MDAM_MODERATELY_DAMAGED: dam_color = BROWN;     break;
            case MDAM_LIGHTLY_DAMAGED:    dam_color = GREEN;     break;
            case MDAM_OKAY:               dam_color = GREEN;     break;
            default:                      dam_color = CYAN; break;
            }
            cprintf(" ");
            textbackground(dam_color);
            textcolor(dam_color);
            // Temporary, to diagnose 1933260
            cprintf("_");
            textbackground(BLACK);
            textcolor(LIGHTGREY);
            cprintf(" ");
            printed += 3;
        }
        else
        {
            textcolor(LIGHTGREY);
            cprintf("  ");
            printed += 2;
        }

        if (printed < crawl_view.mlistsz.x)
        {
            int desc_color;
            std::string desc;
            mons[start].to_string(count, desc, desc_color);
            textcolor(desc_color);
            desc.resize(crawl_view.mlistsz.x-printed, ' ');
            cprintf("%s", desc.c_str());
        }
    }

    // Set start to the next un-described monster
    start = end;
    textcolor(LIGHTGREY);
}

#define BOTTOM_JUSTIFY_MONSTER_LIST 0
void update_monster_pane()
{
    const int max_print = crawl_view.mlistsz.y;
    textbackground(BLACK);

    if (max_print <= 0)
        return;

    std::vector<monster_pane_info> mons;
    {
        std::vector<monsters*> visible;
        get_playervisible_monsters(visible);
        for (unsigned int i = 0; i < visible.size(); i++)
        {
            if (Options.target_zero_exp
                || !mons_class_flag( visible[i]->type, M_NO_EXP_GAIN ))
            {
                mons.push_back(monster_pane_info(visible[i]));
            }
        }
    }

    std::sort(mons.begin(), mons.end(), monster_pane_info::less_than);

#if BOTTOM_JUSTIFY_MONSTER_LIST
    // Count how many groups of monsters there are
    unsigned int lines_needed = mons.size();
    for (unsigned int i=1; i < mons.size(); i++)
        if (! monster_pane_info::less_than(mons[i-1], mons[i]))
            -- lines_needed;
    const int skip_lines = std::max<int>(0, crawl_view.mlistsz.y-lines_needed);
#else
    const int skip_lines = 0;
#endif

    // Print the monsters!

    std::string blank; blank.resize(crawl_view.mlistsz.x, ' ');
    int i_mons = 0;
    for (int i_print = 0; i_print < max_print; ++i_print)
    {
        cgotoxy(1, 1+i_print, GOTO_MLIST);
        // i_mons is incremented by _print_next_monster_desc
        if ((i_print >= skip_lines) && (i_mons < (int)mons.size()))
            _print_next_monster_desc(mons, i_mons);
        else
            cprintf("%s", blank.c_str());
    }

    if (i_mons < (int)mons.size())
    {
        // Didn't get to all of them.
        cgotoxy(crawl_view.mlistsz.x-4, crawl_view.mlistsz.y, GOTO_MLIST);
        cprintf(" ... ");
    }
}
#else
// FIXME: implement this for tiles
void update_monster_pane() {}
#endif

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
    {
        return "Barding";
    }

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

static const char* _determine_color_string( int level )
{
    switch ( level )
    {
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

// old overview screen, now only used for dumping
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
             you.your_name,
             player_title().c_str(),
             score.c_str(),
             species_name(you.species,you.experience_level).c_str(),
             you.class_name,
             god_colour_tag,
             godpowers.c_str(),
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
                 make_time_string(curr, true).c_str(), you.num_turns );
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
             _determine_color_string(rfire), itosym3(rfire),
             _determine_color_string(rcold), itosym3(rcold),
             _determine_color_string(rlife), itosym3(rlife),
             _determine_color_string(rpois), itosym1(rpois),
             _determine_color_string(relec), itosym1(relec));
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
             _determine_color_string(rsust), itosym1(rsust),
             _determine_color_string(rmuta), itosym1(rmuta),
             _determine_color_string(rslow), itosym1(rslow),
             _determine_color_string(rclar), itosym1(rclar));
    cols.add_formatted(1, buf, true);

    {
        const int e_order[] =
        {
            EQ_WEAPON, EQ_BODY_ARMOUR, EQ_SHIELD, EQ_HELMET, EQ_CLOAK,
            EQ_GLOVES, EQ_BOOTS, EQ_AMULET, EQ_RIGHT_RING, EQ_LEFT_RING
        };

        for (int i = 0; i < NUM_EQUIP; i++)
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
             _determine_color_string(rinvi), itosym1(rinvi),
             _determine_color_string(rward), itosym1(rward),
             _determine_color_string(rcons), itosym1(rcons),
             _determine_color_string(rcorr), itosym1(rcorr));
    cols.add_formatted(2, buf, false);

    int saplevel = player_mutation_level(MUT_SAPROVOROUS);
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
             _determine_color_string(saplevel), pregourmand, postgourmand);
    cols.add_formatted(2, buf, false);

    cols.add_formatted(2, " \n", false);

    if ( scan_randarts(RAP_PREVENT_TELEPORTATION, calc_unid) )
    {
        snprintf(buf, sizeof buf, "%sPrev.Telep.: %s",
                 _determine_color_string(-1), itosym1(1));
    }
    else
    {
        const int rrtel = !!player_teleport(calc_unid);
        snprintf(buf, sizeof buf, "%sRnd.Telep. : %s",
                 _determine_color_string(rrtel), itosym1(rrtel));
    }
    cols.add_formatted(2, buf, false);

    const int rctel = player_control_teleport(calc_unid);
    const int rlevi = player_is_airborne();
    const int rcfli = wearing_amulet(AMU_CONTROLLED_FLIGHT, calc_unid);
    snprintf(buf, sizeof buf,
             "%sCtrl.Telep.: %s\n"
             "%sLevitation : %s\n"
             "%sCtrl.Flight: %s\n",
             _determine_color_string(rctel), itosym1(rctel),
             _determine_color_string(rlevi), itosym1(rlevi),
             _determine_color_string(rcfli), itosym1(rcfli));
    cols.add_formatted(2, buf, false);

    return cols.formatted_lines();
}

static std::string _status_mut_abilities(void);

// helper for print_overview_screen
static void _print_overview_screen_equip(column_composer& cols,
                                         std::vector<char>& equip_chars)
{
    const int e_order[] =
    {
        EQ_WEAPON, EQ_BODY_ARMOUR, EQ_SHIELD, EQ_HELMET, EQ_CLOAK,
        EQ_GLOVES, EQ_BOOTS, EQ_AMULET, EQ_RIGHT_RING, EQ_LEFT_RING
    };

    char buf[100];
    for (int i = 0; i < NUM_EQUIP; i++)
    {
        int eqslot = e_order[i];

        char slot_name_lwr[15];
        snprintf(slot_name_lwr, sizeof slot_name_lwr, "%s", equip_slot_to_name(eqslot));
        strlwr(slot_name_lwr);

        char slot[15] = "";
        // uncomment (and change 42 to 33) to bring back slot names
        // snprintf(slot, sizeof slot, "%-7s: ", equip_slot_to_name(eqslot);

        if ( you.equip[ e_order[i] ] != -1)
        {
            const int item_idx = you.equip[e_order[i]];
            const item_def& item = you.inv[item_idx];
            const char* colname = colour_to_str(item.colour);
            const char equip_char = index_to_letter(item_idx);

            char buf2[50];
            if (item.inscription.empty())
                buf2[0] = 0;
            else
                snprintf(buf2, sizeof buf2, " {%s}", item.inscription.c_str());

            snprintf(buf, sizeof buf,
                     "%s<w>%c</w> - <%s>%s</%s>%s",
                     slot,
                     equip_char,
                     colname,
                     item.name(DESC_PLAIN, true).substr(0,42).c_str(),
                     colname,
                     buf2);
            equip_chars.push_back(equip_char);
        }
        else if (e_order[i] == EQ_WEAPON
                 && you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
        {
            snprintf(buf, sizeof buf, "%s  - Blade Hands", slot);
        }
        else if (e_order[i] == EQ_WEAPON
                 && you.skills[SK_UNARMED_COMBAT])
        {
            snprintf(buf, sizeof buf, "%s  - Unarmed", slot);
        }
        else if (!you_can_wear(e_order[i], true))
        {
            snprintf(buf, sizeof buf,
                     "%s<darkgray>(%s unavailable)</darkgray>",
                     slot, slot_name_lwr);
        }
        else if (!you_tran_can_wear(e_order[i], true))
        {
            snprintf(buf, sizeof buf,
                     "%s<darkgray>(%s currently unavailable)</darkgray>",
                     slot, slot_name_lwr);
        }
        else if (!you_can_wear(e_order[i]))
        {
            snprintf(buf, sizeof buf,
                     "%s<lightgray>(%s restricted)</lightgray>",
                     slot, slot_name_lwr);
        }
        else
        {
            snprintf(buf, sizeof buf,
                     "<darkgray>(no %s)</darkgray>", slot_name_lwr);
        }
        cols.add_formatted(2, buf, false);
    }
}

static std::string _overview_screen_title()
{
    char title[50];
    snprintf(title, sizeof title, " the %s ", player_title().c_str());

    char race_class[50];
    snprintf(race_class, sizeof race_class,
             "(%s %s)",
             species_name(you.species, you.experience_level).c_str(),
             you.class_name);

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

    std::string text;
    text = "<yellow>";
    text += you.your_name;
    text += title;
    text += race_class;
    text += std::string(get_number_of_cols() - linelength - 1, ' ');
    text += time_turns;
    text += "</yellow>\n";

    return text;
}

// new scrollable status overview screen,
// including stats, mutations etc.
std::vector<MenuEntry *> _get_overview_screen_results()
{
    bool calc_unid = false;
    formatted_scroller overview;
    // Set flags, and don't use easy exit.
    overview.set_flags(MF_SINGLESELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP, false);
    overview.set_more( formatted_string::parse_string(
                       "<cyan>[ + : Page down.   - : Page up.   Esc exits.]"));
    overview.set_tag("resists");

    overview.add_text(_overview_screen_title());

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

    snprintf(buf, sizeof buf, "Gold %d", you.gold);
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

    snprintf(buf, sizeof buf, "AC %2d" , player_AC());
    cols1.add_formatted(2, buf, false);
    if (you.equip[EQ_SHIELD] == -1)
    {
        textcolor( DARKGREY );
        snprintf(buf, sizeof buf, "Sh  <darkgrey>-</darkgrey>");
    }
    else
    {
        snprintf(buf, sizeof buf, "Sh %2d", player_shield_class());
    }
    cols1.add_formatted(2, buf, false);
    snprintf(buf, sizeof buf, "Ev %2d" , player_evasion());
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
            if ( prank < 0 || you.religion == GOD_XOM)
                prank = 0;
            // Careful about overflow. We erase some of the god's name
            // if necessary.
            godpowers = godpowers.substr(0, 29 - prank) + " " +
                std::string(prank, '*');
        }
    }

    int xp_needed = (exp_needed(you.experience_level + 2) - you.experience) + 1;
    snprintf(buf, sizeof buf,
             "Exp: %d/%lu (%d)%s\n"
             "God: %s%s<lightgrey>\n"
             "Spells: %2d memorised, %2d level%s left\n",
             you.experience_level, you.experience, you.exp_available,
             (you.experience_level < 27?
              make_stringf(", need: %d", xp_needed).c_str() : ""),
             god_colour_tag, godpowers.c_str(),
             you.spell_no, player_spell_levels(), (player_spell_levels() == 1) ? "" : "s");
    cols1.add_formatted(3, buf, false);

    {
        std::vector<formatted_string> blines = cols1.formatted_lines();
        for (unsigned int i = 0; i < blines.size(); ++i )
            overview.add_item_formatted_string(blines[i]);
        overview.add_text(" ");
    }

    // 3 columns, splits at columns 21, 38
    column_composer cols(3, 21, 38);

    const int rfire = player_res_fire(calc_unid);
    const int rcold = player_res_cold(calc_unid);
    const int rlife = player_prot_life(calc_unid);
    const int rpois = player_res_poison(calc_unid);
    const int relec = player_res_electricity(calc_unid);
    const int rsust = player_sust_abil(calc_unid);
    const int rmuta = wearing_amulet(AMU_RESIST_MUTATION, calc_unid)
                      || player_mutation_level(MUT_MUTATION_RESISTANCE) == 3
                      || you.religion == GOD_ZIN && you.piety >= 150;

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
             _determine_color_string(rfire), itosym3(rfire),
             _determine_color_string(rcold), itosym3(rcold),
             _determine_color_string(rlife), itosym3(rlife),
             _determine_color_string(rpois), itosym1(rpois),
             _determine_color_string(relec), itosym1(relec),
             _determine_color_string(rsust), itosym1(rsust),
             _determine_color_string(rmuta), itosym1(rmuta),
             _determine_color_string(rslow), itosym1(rslow));
    cols.add_formatted(0, buf, false);

    int saplevel = player_mutation_level(MUT_SAPROVOROUS);
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
             _determine_color_string(saplevel), pregourmand, postgourmand);
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
             _determine_color_string(rinvi), itosym1(rinvi),
             _determine_color_string(rward), itosym1(rward),
             _determine_color_string(rcons), itosym1(rcons),
             _determine_color_string(rcorr), itosym1(rcorr),
             _determine_color_string(rclar), itosym1(rclar));
    cols.add_formatted(1, buf, false);

    if ( scan_randarts(RAP_PREVENT_TELEPORTATION, calc_unid) )
        snprintf(buf, sizeof buf, "\n%sPrev.Telep.: %s",
                 _determine_color_string(-1), itosym1(1));
    else
    {
        const int rrtel = !!player_teleport(calc_unid);
        snprintf(buf, sizeof buf, "\n%sRnd.Telep. : %s",
                 _determine_color_string(rrtel), itosym1(rrtel));
    }
    cols.add_formatted(1, buf, false);

    const int rctel = player_control_teleport(calc_unid);
    const int rlevi = player_is_airborne();
    const int rcfli = wearing_amulet(AMU_CONTROLLED_FLIGHT, calc_unid);
    snprintf(buf, sizeof buf,
             "%sCtrl.Telep.: %s\n"
             "%sLevitation : %s\n"
             "%sCtrl.Flight: %s\n",
             _determine_color_string(rctel), itosym1(rctel),
             _determine_color_string(rlevi), itosym1(rlevi),
             _determine_color_string(rcfli), itosym1(rcfli));
    cols.add_formatted(1, buf, false);

    std::vector<char> equip_chars;
    _print_overview_screen_equip(cols, equip_chars);

    {
        std::vector<formatted_string> blines = cols.formatted_lines();
        for (unsigned int i = 0; i < blines.size(); ++i )
        {
            // Kind of a hack -- we don't care really what items these
            // hotkeys go to.  So just pick the first few.
            const char hotkey = (i < equip_chars.size()) ? equip_chars[i] : 0;
            overview.add_item_formatted_string(blines[i], hotkey);
        }
    }

    overview.add_text(" ");
    overview.add_text(_status_mut_abilities());
    return overview.show();
}

void print_overview_screen()
{
    while (true)
    {
        std::vector<MenuEntry *> results = _get_overview_screen_results();
        if (results.size() == 0)
        {
            redraw_screen();
            break;
        }

        const char c = results[0]->hotkeys[0];
        item_def& item = you.inv[letter_to_index(c)];
        describe_item(item, true);
        // loop around for another go.
    }
}

// creates rows of short descriptions for current
// status, mutations and abilities
std::string _status_mut_abilities()
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

    if (you.disease && !you.duration[DUR_REGENERATION]
        || you.species == SP_VAMPIRE && you.hunger_state == HS_STARVING
            && you.attribute[ATTR_TRANSFORMATION] != TRAN_BAT)
    {
       text += "non-regenerating, ";
    }
    else if (you.duration[DUR_REGENERATION]
             || you.species == SP_VAMPIRE && you.hunger_state != HS_SATIATED)
    {
        if (you.disease)
            text += "recuperating";
        else
            text += "regenerating";

        if (you.species == SP_VAMPIRE && you.hunger_state != HS_SATIATED)
        {
            if (you.hunger_state < HS_SATIATED)
                text += " slowly";
            else if (you.hunger_state >= HS_FULL)
                text += " quickly";
            else if (you.hunger_state == HS_ENGORGED)
                text += " very quickly";
        }
        text += ", ";
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

    // how exactly did you get to show the status?
    if (you.duration[DUR_PARALYSIS])
        text += "paralysed, ";
    if (you.duration[DUR_SLEEP])
        text += "sleeping, ";

    if (you.duration[DUR_EXHAUSTED])
        text += "exhausted, ";

    if (you.duration[DUR_MIGHT])
        text += "mighty, ";

    if (you.duration[DUR_BERSERKER])
        text += "berserking, ";

    if (player_is_airborne())
        text += "levitating, ";

    if (you.duration[DUR_BARGAIN])
        text += "charismatic, ";

    if (you.duration[DUR_SLAYING])
        text += "deadly, ";

    // DUR_STEALTHY handled in stealth printout

    if (you.duration[DUR_SAGE])
    {
        text += "studying ";
        text += skill_name(you.sage_bonus_skill);
        text += ", ";
    }

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
/*
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
*/
    if (you.duration[DUR_DEATHS_DOOR])
        text += "\nYou are standing in death's doorway.";

    //----------------------------
    // print mutation information
    //----------------------------
    text += "\n<w>A:</w> ";

    int AC_change  = 0;
    int EV_change  = 0;
    int Str_change = 0;
    int Int_change = 0;
    int Dex_change = 0;

    std::vector<std::string> mutations;

    switch (you.species)   //mv: following code shows innate abilities - if any
    {
      case SP_MERFOLK:
          mutations.push_back("change form in water");
          break;

      case SP_NAGA:
          // breathe poison replaces spit poison:
          if (!player_mutation_level(MUT_BREATHE_POISON))
              mutations.push_back("spit poison");
          else
              mutations.push_back("breathe poison");
          break;

      case SP_KENKU:
          mutations.push_back("cannot wear helmets");
          if (you.experience_level > 4)
          {
              std::string help = "able to fly";
              if (you.experience_level > 14)
                  help += " continuously";
              mutations.push_back(help);
          }
          break;

      case SP_VAMPIRE:
          if (you.experience_level < 13 || you.hunger_state >= HS_SATIATED)
              break;
          // else fall-through
      case SP_MUMMY:
          mutations.push_back("in touch with death");
          break;

      case SP_GREY_DRACONIAN:
          if (you.experience_level > 6)
              mutations.push_back("spiky tail");
          break;

      case SP_GREEN_DRACONIAN:
          if (you.experience_level > 6)
              mutations.push_back("breathe poison");
          break;

      case SP_RED_DRACONIAN:
          if (you.experience_level > 6)
              mutations.push_back("breathe fire");
          break;

      case SP_WHITE_DRACONIAN:
          if (you.experience_level > 6)
              mutations.push_back("breathe frost");
          break;

      case SP_BLACK_DRACONIAN:
          if (you.experience_level > 6)
              mutations.push_back("breathe lightning");
          break;

      case SP_GOLDEN_DRACONIAN:
          if (you.experience_level > 6)
          {
              mutations.push_back("spit acid");
              mutations.push_back("acid resistance");
          }
          break;

      case SP_PURPLE_DRACONIAN:
          if (you.experience_level > 6)
              mutations.push_back("breathe power");
          break;

      case SP_MOTTLED_DRACONIAN:
          if (you.experience_level > 6)
              mutations.push_back("breathe sticky flames");
          break;

      case SP_PALE_DRACONIAN:
          if (you.experience_level > 6)
              mutations.push_back("breathe steam");
          break;

      default:
          break;
    }                           //end switch - innate abilities

    // a bit more stuff
    if ( you.species >= SP_OGRE && you.species <= SP_OGRE_MAGE
         || player_genus(GENPC_DRACONIAN) || you.species == SP_SPRIGGAN )
    {
        mutations.push_back("unfitting armour");
    }

    if ( beogh_water_walk() )
        mutations.push_back("water walking");

    std::string current;
    for (unsigned i = 0; i < NUM_MUTATIONS; i++)
    {
        int level = player_mutation_level((mutation_type) i);
        if (!level)
            continue;

        current = "";
        bool lowered = (level < you.mutation[i]);
        switch (i)
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
                    current = "repel missiles";
                break;
            case MUT_POISON_RESISTANCE:
                current = "poison resistance";
                break;
            case MUT_SAPROVOROUS:
                snprintf(info, INFO_SIZE, "saprovore %d", level);
                current = info;
                break;
            case MUT_CARNIVOROUS:
                snprintf(info, INFO_SIZE, "carnivore %d", level);
                current = info;
                break;
            case MUT_HERBIVOROUS:
                snprintf(info, INFO_SIZE, "herbivore %d", level);
                current = info;
                break;
            case MUT_HEAT_RESISTANCE:
                snprintf(info, INFO_SIZE, "fire resistance %d", level);
                current = info;
                break;
            case MUT_COLD_RESISTANCE:
                snprintf(info, INFO_SIZE, "cold resistance %d", level);
                current = info;
                break;
            case MUT_SHOCK_RESISTANCE:
                current = "electricity resistance";
                break;
            case MUT_REGENERATION:
                snprintf(info, INFO_SIZE, "regeneration %d", level);
                current = info;
                break;
            case MUT_FAST_METABOLISM:
                snprintf(info, INFO_SIZE, "fast metabolism %d", level);
                current = info;
                break;
            case MUT_SLOW_METABOLISM:
                snprintf(info, INFO_SIZE, "slow metabolism %d", level);
                current = info;
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
                current = "teleport control";
                break;
            case MUT_TELEPORT:
                snprintf(info, INFO_SIZE, "teleportitis %d", level);
                current = info;
                break;
            case MUT_MAGIC_RESISTANCE:
                snprintf(info, INFO_SIZE, "magic resistance %d", level);
                current = info;
                break;
            case MUT_FAST:
                snprintf(info, INFO_SIZE, "speed %d", level);
                current = info;
                break;
            case MUT_ACUTE_VISION:
                current = "see invisible";
                break;
            case MUT_DEFORMED:
                snprintf(info, INFO_SIZE, "deformed body %d", level);
                current = info;
                break;
            case MUT_TELEPORT_AT_WILL:
                snprintf(info, INFO_SIZE, "teleport at will %d", level);
                current = info;
                break;
            case MUT_SPIT_POISON:
                current = "spit poison";
                break;
            case MUT_MAPPING:
                snprintf(info, INFO_SIZE, "sense surroundings %d", level);
                current = info;
                break;
            case MUT_BREATHE_FLAMES:
                snprintf(info, INFO_SIZE, "breathe flames %d", level);
                current = info;
                break;
            case MUT_BLINK:
                current = "blink";
                break;
            case MUT_HORNS:
                snprintf(info, INFO_SIZE, "horns %d", level);
                current = info;
                break;
            case MUT_STRONG_STIFF:
                Str_change += level;
                Dex_change -= level;
                break;
            case MUT_FLEXIBLE_WEAK:
                Str_change -= level;
                Dex_change += level;
                break;
            case MUT_SCREAM:
                snprintf(info, INFO_SIZE, "screaming %d", level);
                current = info;
                break;
            case MUT_CLARITY:
                snprintf(info, INFO_SIZE, "clarity %d", level);
                current = info;
                break;
            case MUT_BERSERK:
                snprintf(info, INFO_SIZE, "berserk %d", level);
                current = info;
                break;
            case MUT_DETERIORATION:
                snprintf(info, INFO_SIZE, "deterioration %d", level);
                current = info;
                break;
            case MUT_BLURRY_VISION:
                snprintf(info, INFO_SIZE, "blurry vision %d", level);
                current = info;
                break;
            case MUT_MUTATION_RESISTANCE:
                snprintf(info, INFO_SIZE, "mutation resistance %d", level);
                current = info;
                break;
            case MUT_FRAIL:
                snprintf(info, INFO_SIZE, "-%d%% hp", level*10);
                current = info;
                break;
            case MUT_ROBUST:
                snprintf(info, INFO_SIZE, "+%d%% hp", level*10);
                current = info;
                break;
            case MUT_LOW_MAGIC:
                snprintf(info, INFO_SIZE, "-%d%% mp", level*10);
                current = info;
                break;
            case MUT_HIGH_MAGIC:
                snprintf(info, INFO_SIZE, "+%d%% mp", level*10);
                current = info;
                break;

            /* demonspawn mutations */
            case MUT_TORMENT_RESISTANCE:
                current = "torment resistance";
                break;
            case MUT_NEGATIVE_ENERGY_RESISTANCE:
                snprintf(info, INFO_SIZE, "life protection %d", level);
                current = info;
                break;
            case MUT_SUMMON_MINOR_DEMONS:
                current = "summon minor demons";
                break;
            case MUT_SUMMON_DEMONS:
                current = "summon demons";
                break;
            case MUT_HURL_HELLFIRE:
                current = "hurl hellfire";
                break;
            case MUT_CALL_TORMENT:
                current = "call torment";
                break;
            case MUT_RAISE_DEAD:
                current = "raise dead";
                break;
            case MUT_CONTROL_DEMONS:
                current = "control demons";
                break;
            case MUT_PANDEMONIUM:
                current = "portal to Pandemonium";
                break;
            case MUT_DEATH_STRENGTH:
                current = "draw strength from death and destruction";
                break;
            case MUT_CHANNEL_HELL:
                current = "channel magical energy from Hell";
                break;
            case MUT_DRAIN_LIFE:
                current = "drain life";
                break;
            case MUT_THROW_FLAMES:
                current = "throw flames of Gehenna";
                break;
            case MUT_THROW_FROST:
                current = "throw frost of Cocytus";
                break;
            case MUT_SMITE:
                current = "invoke powers of Tartarus";
                break;
            /* end of demonspawn mutations */

            case MUT_CLAWS:
                snprintf(info, INFO_SIZE, "claws %d", level);
                current = info;
                break;
            case MUT_FANGS:
                snprintf(info, INFO_SIZE, "sharp teeth %d", level);
                current = info;
                break;
            case MUT_HOOVES:
                current = "hooves";
                break;
            case MUT_TALONS:
                current = "talons";
                break;
            case MUT_BREATHE_POISON:
                current = "breathe poison";
                break;
            case MUT_STINGER:
                snprintf(info, INFO_SIZE, "stinger %d", level);
                current = info;
                break;
            case MUT_BIG_WINGS:
                current = "large and strong wings";
                break;
            case MUT_BLUE_MARKS:
                snprintf(info, INFO_SIZE, "blue evil mark %d", level);
                current = info;
                break;
            case MUT_GREEN_MARKS:
                snprintf(info, INFO_SIZE, "green evil mark %d", level);
                current = info;
                break;

            // scales etc. -> calculate sum of AC bonus
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
            case MUT_SHAGGY_FUR:
                AC_change += level;
                if (level == 3)
                    current = "shaggy fur";
                break;
            default: break;
        }

        if (!current.empty())
        {
            if (lowered)
                current = "<darkgrey>" + current + "</darkgrey>";
            mutations.push_back(current);
        }
    }

    if (AC_change)
    {
        snprintf(info, INFO_SIZE, "AC %s%d", (AC_change > 0 ? "+" : ""), AC_change);
        mutations.push_back(info);
    }
    if (EV_change)
    {
        snprintf(info, INFO_SIZE, "EV +%d", EV_change);
        mutations.push_back(info);
    }
    if (Str_change)
    {
        snprintf(info, INFO_SIZE, "Str %s%d", (Str_change > 0 ? "+" : ""), Str_change);
        mutations.push_back(info);
    }
    if (Int_change)
    {
        snprintf(info, INFO_SIZE, "Int %s%d", (Int_change > 0 ? "+" : ""), Int_change);
        mutations.push_back(info);
    }
    if (Dex_change)
    {
        snprintf(info, INFO_SIZE, "Dex %s%d", (Dex_change > 0 ? "+" : ""), Dex_change);
        mutations.push_back(info);
    }

    if (mutations.empty())
        text +=  "no striking features";
    else
    {
        text += comma_separated_line(mutations.begin(), mutations.end(),
                                     ", ", ", ");
    }

    //----------------------------
    // print ability information
    //----------------------------

    text += print_abilities();
    linebreak_string2(text, get_number_of_cols());

    return text;
}

