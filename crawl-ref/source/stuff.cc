/*
 *  File:       stuff.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <4>    11/14/99     cdl    added random40(), made arg to random*() signed
 *   <3>    11/06/99     cdl    added random22()
 *   <2>     9/25/99     cdl    linuxlib -> liblinux
 *   <1>     -/--/--     LRH    Created
 */

#include "AppHdr.h"
#include "database.h"
#include "direct.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "state.h"
#include "stuff.h"
#include "view.h"

#include <cstdarg>
#include <sstream>
#include <iomanip>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <stack>

#ifdef USE_MORE_SECURE_SEED

// for times()
#include <sys/times.h>

// for getpid()
#include <sys/types.h>
#include <unistd.h>

#endif

#ifdef DOS
#include <conio.h>
#endif

#ifdef UNIX
#include "libunix.h"
#endif

#include "branch.h"
#include "delay.h"
#include "externs.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mt19937ar.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "skills2.h"
#include "tutorial.h"
#include "view.h"

// Crude, but functional.
std::string make_time_string( time_t abs_time, bool terse )
{
    const int days  = abs_time / 86400;
    const int hours = (abs_time % 86400) / 3600;
    const int mins  = (abs_time % 3600) / 60;
    const int secs  = abs_time % 60;

    std::ostringstream buff;
    buff << std::setfill('0');

    if (days > 0)
    {
        if (terse)
            buff << days << ", ";
        else
            buff << days << (days > 1 ? " days" : "day");
    }

    buff << std::setw(2) << hours << ':'
         << std::setw(2) << mins << ':'
         << std::setw(2) << secs;
    return buff.str();
}

void set_redraw_status( unsigned long flags )
{
    you.redraw_status_flags |= flags;
}

static bool tag_follower_at(const coord_def &pos)
{
    if (!in_bounds(pos) || pos == you.pos())
        return (false);

    if (mgrd(pos) == NON_MONSTER)
        return (false);

    monsters *fmenv = &menv[mgrd(pos)];

    if (fmenv->type == MONS_PLAYER_GHOST
        || !fmenv->alive()
        || mons_is_stationary(fmenv)
        || fmenv->incapacitated())
    {
        return (false);
    }

    if (!monster_habitable_grid(fmenv, DNGN_FLOOR))
        return (false);

    if (fmenv->speed_increment < 50)
        return (false);

    // only friendly monsters, or those actively seeking the
    // player, will follow up/down stairs.
    if (!(mons_friendly(fmenv) ||
          (fmenv->behaviour == BEH_SEEK && fmenv->foe == MHITYOU)))
    {
        return (false);
    }

    // Monsters that are not directly adjacent are subject to more
    // stringent checks.
    if ((pos - you.pos()).abs() > 2)
    {
        if (!mons_friendly(fmenv))
            return (false);

        // Orcs will follow Beogh worshippers.
        if (!(mons_species(fmenv->type) == MONS_ORC
              && you.religion == GOD_BEOGH))
            return (false);
    }

    // monster is chasing player through stairs:
    fmenv->flags |= MF_TAKING_STAIRS;

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "%s is marked for following.",
         fmenv->name(DESC_CAP_THE, true).c_str() );
#endif
    return (true);
}

static int follower_tag_radius2()
{
    // If only friendlies are adjacent, we set a max radius of 6, otherwise
    // only adjacent friendlies may follow.
    coord_def p;
    for (p.x = you.x_pos - 1; p.x <= you.x_pos + 1; ++p.x)
        for (p.y = you.y_pos - 1; p.y <= you.y_pos + 1; ++p.y)
        {
            if (p == you.pos())
                continue;
            if (const monsters *mon = monster_at(p))
            {
                if (!mons_friendly(mon))
                    return (2);
            }
        }
    return (6 * 6);
}

void tag_followers()
{
    const int radius2 = follower_tag_radius2();
    int n_followers = 18;

    std::vector<coord_def> places[2];
    int place_set = 0;

    places[place_set].push_back(you.pos());
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    while (!places[place_set].empty())
    {
        for (int i = 0, size = places[place_set].size(); i < size; ++i)
        {
            const coord_def &p = places[place_set][i];

            coord_def fp;
            for (fp.x = p.x - 1; fp.x <= p.x + 1; ++fp.x)
                for (fp.y = p.y - 1; fp.y <= p.y + 1; ++fp.y)
                {
                    if (fp == p || (fp - you.pos()).abs() > radius2
                        || !in_bounds(fp) || travel_point_distance[fp.x][fp.y])
                    {
                        continue;
                    }
                    travel_point_distance[fp.x][fp.y] = 1;
                    if (tag_follower_at(fp))
                    {
                        // If we've run out of our follower allowance, bail.
                        if (--n_followers <= 0)
                            return;
                        places[!place_set].push_back(fp);
                    }
                }
        }
        places[place_set].clear();
        place_set = !place_set;
    }
}

void untag_followers()
{
    for (int m = 0; m < MAX_MONSTERS; m++)
        menv[m].flags &= (~MF_TAKING_STAIRS);
}

unsigned char get_ch()
{
    unsigned char gotched = getch();

    if (gotched == 0)
        gotched = getch();

    return gotched;
}                               // end get_ch()

void seed_rng(long seed)
{
    // MT19937 -- see mt19937ar.cc for details/licence
    init_genrand(seed);
}

void seed_rng()
{
    unsigned long seed = time( NULL );
#ifdef USE_MORE_SECURE_SEED
    struct tms  buf;
    seed += times( &buf ) + getpid();
#endif

    seed_rng(seed);
}

// MT19937 -- see mt19937ar.cc for details
unsigned long random_int( void )
{
    return (genrand_int32());
}

int random_range(int low, int high)
{
    ASSERT(low <= high);
    return (low + random2(high - low + 1));
}

int random_range(int low, int high, int nrolls)
{
    ASSERT(nrolls > 0);
    int sum = 0;
    for (int i = 0; i < nrolls; ++i)
        sum += random_range(low, high);
    return (sum / nrolls);
}

int random_choose(int first, ...)
{
    va_list args;
    va_start(args, first);

    int chosen = first, count = 1, nargs = 100;

    while (nargs-- > 0)
    {
        const int pick = va_arg(args, int);
        if (pick == -1)
            break;
        if (one_chance_in(++count))
            chosen = pick;
    }

    ASSERT(nargs > 0);
    
    va_end(args);
    return (chosen);
}

int random_choose_weighted(int weight, int first, ...)
{
    va_list args;
    va_start(args, first);

    int chosen = first, cweight = weight, nargs = 100;

    while (nargs-- > 0)
    {
        const int nweight = va_arg(args, int);
        if (!nweight)
            break;

        const int choice = va_arg(args, int);
        if (random2(cweight += nweight) < nweight)
            chosen = choice;
    }

    ASSERT(nargs > 0);
    
    va_end(args);
    return (chosen);
}

int random2( int max )
{
    if (max <= 1)
        return (0);

    return (static_cast<int>( genrand_int32() / (0xFFFFFFFFUL / max + 1) ));
}

bool coinflip( void )
{
    return (static_cast<bool>( random2(2) ));
}

void push_rng_state()
{
    push_mt_state();
}

void pop_rng_state()
{
    pop_mt_state();
}

// Attempts to make missile weapons nicer to the player by
// reducing the extreme variance in damage done.
void scale_dice( dice_def &dice, int threshold )
{
    while (dice.size > threshold)
    {
        dice.num *= 2;
        // If it's an odd number, lose one; this is more than
        // compensated by the increase in number of dice.
        dice.size /= 2;
    }
}

int bestroll(int max, int rolls)
{
    int best = 0;
    for (int i = 0; i < rolls; i++)
    {
        int curr = random2(max);
        if (curr > best)
            best = curr;
    }
    return (best);
}

// random2avg() returns same mean value as random2() but with a  lower variance
// never use with rolls < 2 as that would be silly - use random2() instead {dlb}
int random2avg(int max, int rolls)
{
    int sum = 0;

    sum += random2(max);

    for (int i = 0; i < (rolls - 1); i++)
    {
        sum += random2(max + 1);
    }

    return (sum / rolls);
}

int roll_dice( int num, int size )
{
    int ret = 0;
    int i;

    // If num <= 0 or size <= 0, then we'll just return the default 
    // value of zero.  This is good behaviour in that it will be 
    // appropriate for calculated values that might be passed in.
    if (num > 0 && size > 0)
    {
        ret += num;     // since random2() is zero based

        for (i = 0; i < num; i++)
            ret += random2( size );
    }

    return (ret);
}

int roll_dice( const struct dice_def &dice )
{
    return (roll_dice( dice.num, dice.size ));
}

// originally designed to randomize evasion -
// values are slightly lowered near (max) and
// approach an upper limit somewhere near (limit/2)
int random2limit(int max, int limit)
{
    int i;
    int sum = 0;

    if (max < 1)
        return 0;

    for (i = 0; i < max; i++)
    {
        if (random2(limit) >= i)
            sum++;
    }

    return sum;
}                               // end random2limit()

void cio_init()
{
    crawl_state.io_inited = true;

#ifdef UNIX
    unixcurses_startup();
#endif

#ifdef WIN32CONSOLE
    init_libw32c();
#endif

#ifdef DOS
    init_libdos();
#endif
    
    crawl_view.init_geometry();

    if (Options.char_set == CSET_UNICODE && !crawl_state.unicode_ok)
    {
        crawl_state.add_startup_error(
            "Unicode glyphs are not available, falling back to ASCII.");
        Options.char_set = CSET_ASCII;
    }
}

void cio_cleanup()
{
    if (!crawl_state.io_inited)
        return;
    
#ifdef UNIX
    unixcurses_shutdown();
#endif

#ifdef WIN32CONSOLE
    deinit_libw32c();
#endif

    msg::deinitalise_mpr_streams();

    crawl_state.io_inited = false;
}

void end(int exit_code, bool print_error, const char *format, ...)
{
    std::string error = print_error? strerror(errno) : "";
    
    cio_cleanup();
    databaseSystemShutdown();
    if (format)
    {
        va_list arg;
        va_start(arg, format);
        char buffer[100];
        vsnprintf(buffer, sizeof buffer, format, arg);
        va_end(arg);
        
        if (error.empty())
            error = std::string(buffer);
        else
            error = std::string(buffer) + ": " + error;
    }

    if (!error.empty())
    {
        if (error[error.length() - 1] != '\n')
            error += "\n";
        fprintf(stderr, "%s", error.c_str());
        error.clear();
    }

#if defined(WIN32CONSOLE) || defined(DOS) || defined(DGL_PAUSE_AFTER_ERROR)
    if (exit_code)
    {
        fprintf(stderr, "Hit Enter to continue...\n");
        getchar();
    }
#endif
    
    exit(exit_code);
}

void redraw_screen(void)
{
    draw_border();

    you.redraw_hit_points = true;
    you.redraw_magic_points = true;
    you.redraw_strength = true;
    you.redraw_intelligence = true;
    you.redraw_dexterity = true;
    you.redraw_armour_class = true;
    you.redraw_evasion = true;
    you.redraw_gold = true;
    you.redraw_experience = true;
    you.wield_change = true;

    set_redraw_status(
        REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK );

    print_stats();

    if (Options.delay_message_clear)
        mesclr( true );

    bool note_status = notes_are_active();
    activate_notes(false);
    new_level();
#ifdef DGL_SIMPLE_MESSAGING
    update_message_status();
#endif
    update_turn_count();
    activate_notes(note_status);

    viewwindow(1, false);
}                               // end redraw_screen()

// STEPDOWN FUNCTION to replace conditional chains in spells2.cc 12jan2000 {dlb}
// it is a bit more extensible and optimizes the logical structure, as well
// usage: summon_swarm() summon_undead() summon_scorpions() summon_things()
// ex(1): stepdown_value (foo, 2, 2, 6, 8) replaces the following block:
//

/*
   if (foo > 2)
     foo = (foo - 2) / 2 + 2;
   if (foo > 4)
     foo = (foo - 4) / 2 + 4;
   if (foo > 6)
     foo = (foo - 6) / 2 + 6;
   if (foo > 8)
     foo = 8;
 */

//
// ex(2): bar = stepdown_value(bar, 2, 2, 6, -1) replaces the following block:
//

/*
   if (bar > 2)
     bar = (bar - 2) / 2 + 2;
   if (bar > 4)
     bar = (bar - 4) / 2 + 4;
   if (bar > 6)
     bar = (bar - 6) / 2 + 6;
 */

// I hope this permits easier/more experimentation with value stepdowns in
// the code it really needs to be rewritten to accept arbitrary (unevenly
// spaced) steppings
int stepdown_value(int base_value, int stepping, int first_step,
                   int last_step, int ceiling_value)
{
    int return_value = base_value;

    // values up to the first "step" returned unchanged:
    if (return_value <= first_step)
        return return_value;

    for (int this_step = first_step; this_step <= last_step;
                                                    this_step += stepping)
    {
        if (return_value > this_step)
            return_value = ((return_value - this_step) / 2) + this_step;
        else
            break;              // exit loop iff value fully "stepped down"
    }

    // "no final ceiling" == -1
    if (ceiling_value != -1 && return_value > ceiling_value)
        return ceiling_value;   // highest value to return is "ceiling"
    else
        return return_value;    // otherwise, value returned "as is"

}                               // end stepdown_value()

int skill_bump( int skill )
{   
    return ((you.skills[skill] < 3) ? you.skills[skill] * 2
                                    : you.skills[skill] + 3);
}

// This gives (default div = 20, shift = 3):
// - shift/div% @ stat_level = 0; (default 3/20 = 15%, or 20% at stat 1)
// - even (100%) @ stat_level = div - shift; (default 17)
// - 1/div% per stat_level (default 1/20 = 5%)
int stat_mult( int stat_level, int value, int div, int shift )
{
    return (((stat_level + shift) * value) / ((div > 1) ? div : 1));
}

// As above but inverted (ie 5x penalty at stat 1)
int stat_div( int stat_level, int value, int mult, int shift )
{
    int div = stat_level + shift;

    if (div < 1)
        div = 1;

    return ((mult * value) / div);
}

// Calculates num/den and randomly adds one based on the remainder.
int div_rand_round( int num, int den )
{
    return (num / den + (random2(den) < num % den));
}

bool one_chance_in(int a_million)
{
    return (random2(a_million) == 0);
}

// simple little function to quickly modify all three stats
// at once - does check for '0' modifiers to prevent needless
// adding .. could use checking for sums less than zero, I guess.
// used in conjunction with newgame::species_stat_init() and
// newgame::job_stat_init() routines 24jan2000 {dlb}
void modify_all_stats(int STmod, int IQmod, int DXmod)
{
    if (STmod)
    {
        you.strength += STmod;
        you.max_strength += STmod;
        you.redraw_strength = 1;
    }

    if (IQmod)
    {
        you.intel += IQmod;
        you.max_intel += IQmod;
        you.redraw_intelligence = 1;
    }

    if (DXmod)
    {
        you.dex += DXmod;
        you.max_dex += DXmod;
        you.redraw_dexterity = 1;
    }

    return;
}                               // end modify_stat()

void canned_msg(canned_message_type which_message)
{
    switch (which_message)
    {
    case MSG_SOMETHING_APPEARS:
        mprf("Something appears %s!",
             (you.species == SP_NAGA || you.mutation[MUT_HOOVES])
             ? "before you" : "at your feet");
        break;

    case MSG_NOTHING_HAPPENS:
        mpr("Nothing appears to happen.");
        break;
    case MSG_YOU_RESIST:
        mpr("You resist.");
        break;
    case MSG_TOO_BERSERK:
        mpr("You are too berserk!");
        break;
    case MSG_PRESENT_FORM:
        mpr("You can't do that in your present form.");
        break;
    case MSG_NOTHING_CARRIED:
        mpr("You aren't carrying anything.");
        break;
    case MSG_CANNOT_DO_YET:
        mpr("You can't do that yet.");
        break;
    case MSG_OK:
        mpr("Okay, then.");
        break;
    case MSG_UNTHINKING_ACT:
        mpr("Why would you want to do that?");
        break;
    case MSG_SPELL_FIZZLES:
        mpr("The spell fizzles.");
        break;
    case MSG_HUH:
        mpr("Huh?");
        break;
    case MSG_EMPTY_HANDED:
        mpr("You are now empty-handed.");
        break;
    }

    return;
}                               // end canned_msg()

// jmf: general helper (should be used all over in code)
//      -- idea borrowed from Nethack
bool yesno( const char *str, bool safe, int safeanswer, bool clear_after,
            bool interrupt_delays, bool noprompt )
{
    if (interrupt_delays)
        interrupt_activity( AI_FORCE_INTERRUPT );
    for (;;)
    {
        if ( !noprompt )
            mpr(str, MSGCH_PROMPT);

        int tmp = getchm(KC_CONFIRM);

        if ((tmp == ' ' || tmp == 27 || tmp == '\r' || tmp == '\n') 
                && safeanswer)
            tmp = safeanswer;

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || (Options.easy_confirm == CONFIRM_SAFE_EASY && safe))
        {
            tmp = toupper( tmp );
        }

        if (clear_after)
            mesclr();

        if (tmp == 'N')
            return false;
        else if (tmp == 'Y')
            return true;
        else if (!noprompt)
            mpr("[Y]es or [N]o only, please.");
    }
}                               // end yesno()

// like yesno(), but returns 0 for no, 1 for yes, and -1 for quit
int yesnoquit( const char* str, bool safe, int safeanswer, bool clear_after )
{
    interrupt_activity( AI_FORCE_INTERRUPT );
    while (1)
    {
        mpr(str, MSGCH_PROMPT);

        int tmp = getchm(KC_CONFIRM);

        if ( tmp == ESCAPE || tmp == 'q' || tmp == 'Q' )
            return -1;
        
        if ((tmp == ' ' || tmp == '\r' || tmp == '\n') && safeanswer)
            tmp = safeanswer;

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || (Options.easy_confirm == CONFIRM_SAFE_EASY && safe))
        {
            tmp = toupper( tmp );
        }

        if (clear_after)
            mesclr();

        if (tmp == 'N')
            return 0;
        else if (tmp == 'Y')
            return 1;
        else
            mpr("[Y]es or [N]o only, please.");
    }
}    

// More accurate than distance() given the actual movement geometry -- bwr
int grid_distance( int x, int y, int x2, int y2 )
{
    const int dx = abs( x - x2 );
    const int dy = abs( y - y2 );

    // returns distance in terms of moves:
    return ((dx > dy) ? dx : dy);
}

int distance( int x, int y, int x2, int y2 )
{
    //jmf: now accurate, but remember to only compare vs. pre-squared distances
    //     thus, next to == (distance(m1.x,m1.y, m2.x,m2.y) <= 2)
    const int dx = x - x2;
    const int dy = y - y2;

    return ((dx * dx) + (dy * dy));
}                               // end distance()

bool adjacent( int x, int y, int x2, int y2 )
{
    return (abs(x - x2) <= 1 && abs(y - y2) <= 1);
}

bool silenced(int x, int y)
{
    if (you.duration[DUR_SILENCE] > 0
        && distance(x, y, you.x_pos, you.y_pos) <= 36)  // (6 * 6)
    {
        return true;
    }
    else
    {
        //else // FIXME: implement, and let monsters cast, too
        //  for (int i = 0; i < MAX_SILENCES; i++)
        //  {
        //      if (distance(x, y, silencer[i].x, silencer[i].y) <= 36)
        //         return true;
        //  }
        return false;
    }
}                               // end silenced()

bool player_can_hear(int x, int y)
{
    return (!silenced(x, y) && !silenced(you.x_pos, you.y_pos));
}                               // end player_can_hear()

// Returns true if inside the area the player can move and dig (ie exclusive)
bool in_bounds( int x, int y )
{
    return (x > X_BOUND_1 && x < X_BOUND_2 
            && y > Y_BOUND_1 && y < Y_BOUND_2);
}

// Returns true if inside the area the player can map (ie inclusive).
// Note that terrain features should be in_bounds() leaving an outer
// ring of rock to frame the level.
bool map_bounds( int x, int y )
{
    return (x >= X_BOUND_1 && x <= X_BOUND_2
            && y >= Y_BOUND_1 && y <= Y_BOUND_2);
}

// Returns a random location in (x_pos, y_pos)... the grid will be
// DNGN_FLOOR if clear, and NON_MONSTER if empty.  Exclusive tells
// if we're using in_bounds() or map_bounds() restriction.
void random_in_bounds( int &x_pos, int &y_pos, int terr,
                       bool empty, bool excl )
{
    bool done = false;

    do
    {
        x_pos = X_BOUND_1 + random2( X_WIDTH - 2 * excl ) + 1 * excl;
        y_pos = Y_BOUND_1 + random2( Y_WIDTH - 2 * excl ) + 1 * excl;

        if (terr == DNGN_RANDOM)
            done = true;
        else if (terr == grd[x_pos][y_pos])
            done = true;
        else if (terr == DNGN_DEEP_WATER
                 && grd[x_pos][y_pos] == DNGN_SHALLOW_WATER)
            done = true;
        else if (empty 
                && mgrd[x_pos][y_pos] != NON_MONSTER 
                && (x_pos != you.x_pos || y_pos != you.y_pos))
        {
            done = true;
        }
    }
    while (!done);
}

unsigned char random_colour(void)
{
    return (1 + random2(15));
}                               // end random_colour()

unsigned char random_uncommon_colour()
{
    unsigned char result;
    do
        result = random_colour();
    while ( result == LIGHTCYAN || result == CYAN || result == BROWN );
    return result;
}


// returns if a colour is one of the special element colours (ie not regular)
bool is_element_colour( int col )
{
    // striping any COLFLAGS (just in case)
    return ((col & 0x007f) >= EC_FIRE);
}

int element_colour( int element, bool no_random )
{
    // Doing this so that we don't have to do recursion here at all 
    // (these were the only cases which had possible double evaluation):
    if (element == EC_FLOOR)
        element = env.floor_colour;
    else if (element == EC_ROCK)
        element = env.rock_colour;

    // pass regular colours through for safety.
    if (!is_element_colour( element ))
        return (element);

    int ret = BLACK;

    // Setting no_random to true will get the first colour in the cases
    // below.  This is potentially useful for calls to this function 
    // which might want a consistent result.
    int tmp_rand = (no_random ? 0 : random2(120));

    switch (element & 0x007f)   // strip COLFLAGs just in case
    {
    case EC_FIRE:
        ret = (tmp_rand < 40) ? RED :
              (tmp_rand < 80) ? YELLOW 
                              : LIGHTRED;
        break;

    case EC_ICE:
        ret = (tmp_rand < 40) ? LIGHTBLUE :
              (tmp_rand < 80) ? BLUE 
                              : WHITE;
        break;

    case EC_EARTH:
        ret = (tmp_rand < 60) ? BROWN : LIGHTRED;
        break;

    case EC_AIR:
        ret = (tmp_rand < 60) ? LIGHTGREY : WHITE;
        break;

    case EC_ELECTRICITY:
        ret = (tmp_rand < 40) ? LIGHTCYAN :
              (tmp_rand < 80) ? LIGHTBLUE 
                              : CYAN;
        break;

    case EC_POISON:
        ret = (tmp_rand < 60) ? LIGHTGREEN : GREEN;
        break;

    case EC_WATER:
        ret = (tmp_rand < 60) ? BLUE : CYAN;
        break;

    case EC_MAGIC:
        ret = (tmp_rand < 30) ? LIGHTMAGENTA :
              (tmp_rand < 60) ? LIGHTBLUE :
              (tmp_rand < 90) ? MAGENTA 
                              : BLUE;
        break;

    case EC_MUTAGENIC:
    case EC_WARP:
        ret = (tmp_rand < 60) ? LIGHTMAGENTA : MAGENTA;
        break;

    case EC_ENCHANT:
        ret = (tmp_rand < 60) ? LIGHTBLUE : BLUE;
        break;

    case EC_HEAL:
        ret = (tmp_rand < 60) ? LIGHTBLUE : YELLOW;
        break;

    case EC_BLOOD:
        ret = (tmp_rand < 60) ? RED : DARKGREY;
        break;

    case EC_DEATH:      // assassin
    case EC_NECRO:      // necromancer
        ret = (tmp_rand < 80) ? DARKGREY : MAGENTA;
        break;

    case EC_UNHOLY:     // ie demonology
        ret = (tmp_rand < 80) ? DARKGREY : RED;
        break;

    case EC_DARK:
        ret = DARKGREY;
        break;

    case EC_HOLY:
        ret = (tmp_rand < 60) ? YELLOW : WHITE;
        break;

    case EC_VEHUMET:
        ret = (tmp_rand < 40) ? LIGHTRED :
              (tmp_rand < 80) ? LIGHTMAGENTA 
                              : LIGHTBLUE;
        break;

    case EC_BEOGH:
        ret = (tmp_rand < 60) ? LIGHTRED : BROWN;
        break;

    case EC_CRYSTAL:
        ret = (tmp_rand < 40) ? LIGHTGREY :
              (tmp_rand < 80) ? GREEN 
                              : LIGHTRED;
        break;

    case EC_SLIME:
        ret = (tmp_rand < 40) ? GREEN :
              (tmp_rand < 80) ? BROWN 
                              : LIGHTGREEN;
        break;

    case EC_SMOKE:
        ret = (tmp_rand < 30) ? LIGHTGREY :
              (tmp_rand < 60) ? DARKGREY :
              (tmp_rand < 90) ? LIGHTBLUE 
                              : MAGENTA;
        break;

    case EC_JEWEL:
        ret = (tmp_rand <  12) ? WHITE :
              (tmp_rand <  24) ? YELLOW :
              (tmp_rand <  36) ? LIGHTMAGENTA :
              (tmp_rand <  48) ? LIGHTRED :
              (tmp_rand <  60) ? LIGHTGREEN :
              (tmp_rand <  72) ? LIGHTBLUE :
              (tmp_rand <  84) ? MAGENTA :
              (tmp_rand <  96) ? RED :
              (tmp_rand < 108) ? GREEN 
                               : BLUE;
        break;

    case EC_ELVEN:
        ret = (tmp_rand <  40) ? LIGHTGREEN :
              (tmp_rand <  80) ? GREEN :
              (tmp_rand < 100) ? LIGHTBLUE 
                               : BLUE;
        break;

    case EC_DWARVEN:
        ret = (tmp_rand <  40) ? BROWN :
              (tmp_rand <  80) ? LIGHTRED :
              (tmp_rand < 100) ? LIGHTGREY 
                               : CYAN;
        break;

    case EC_ORCISH:
        ret = (tmp_rand <  40) ? DARKGREY :
              (tmp_rand <  80) ? RED :
              (tmp_rand < 100) ? BROWN 
                               : MAGENTA;
        break;

    case EC_GILA:
        ret = (tmp_rand <  30) ? LIGHTMAGENTA :
              (tmp_rand <  60) ? MAGENTA :
              (tmp_rand <  90) ? YELLOW : 
              (tmp_rand < 105) ? LIGHTRED 
                               : RED;
        break;

    case EC_STONE:
        if (player_in_branch( BRANCH_HALL_OF_ZOT ))
            ret = env.rock_colour;
        else
            ret = LIGHTGREY;
        break;

    case EC_MIST:
        ret = tmp_rand < 100? CYAN : BLUE;
        break;

    case EC_SHIMMER_BLUE:
        ret = random_choose_weighted(80, BLUE, 20, LIGHTBLUE, 5, CYAN, 0);
        break;

    case EC_RANDOM:
        ret = 1 + random2(15);              // always random
        break;

    case EC_FLOOR: // should alredy be handled 
    case EC_ROCK:  // should alredy be handled 
    default:
        break;
    }

    ASSERT( !is_element_colour( ret ) );

    return ((ret == BLACK) ? GREEN : ret);
}

char index_to_letter(int the_index)
{
    return (the_index + ((the_index < 26) ? 'a' : ('A' - 26)));
}                               // end index_to_letter()

int letter_to_index(int the_letter)
{
    if (the_letter >= 'a' && the_letter <= 'z')
        // returns range [0-25] {dlb}
        the_letter -= 'a';
    else if (the_letter >= 'A' && the_letter <= 'Z')
        // returns range [26-51] {dlb}
        the_letter -= ('A' - 26);

    return the_letter;
}                               // end letter_to_index()

int fuzz_value(int val, int lowfuzz, int highfuzz, int naverage)
{
    const int lfuzz = lowfuzz * val / 100,
        hfuzz = highfuzz * val / 100;
    return val + random2avg(lfuzz + hfuzz + 1, naverage) - lfuzz;
}

// returns 0 if the point is not near stairs
// returns 1 if the point is near unoccupied stairs
// returns 2 if the point is near player-occupied stairs

int near_stairs(const coord_def &p, int max_dist,
                dungeon_char_type &stair_type,
                branch_type &branch)
{
    coord_def inc;
    for (inc.x = -max_dist; inc.x <= max_dist; inc.x++)
    {
        for(inc.y = -max_dist; inc.y <= max_dist; inc.y++)
        {
            const coord_def np(p + inc);

            if (!in_bounds(np))
                continue;

            const dungeon_feature_type feat = grd(np);
            if (is_stair(feat))
            {
                // shouldn't happen for escape hatches
                if (grid_is_rock_stair(feat))
                    continue;

                stair_type = get_feature_dchar(feat);
                
                // is it a branch stair?
                for (int i = 0; i < NUM_BRANCHES; ++i)
                {
                     if (branches[i].entry_stairs == feat)
                     {
                         branch = branches[i].id;
                         break;
                     }
                     else if (branches[i].exit_stairs == feat)
                     {
                         branch = branches[i].parent_branch;
                         break;
                     }
                }
                return (np == you.pos()? 2 : 1);
            }
        }
    }

    return false;
}

bool is_trap_square(int x, int y)
{
    return (grd[x][y] >= DNGN_TRAP_MECHANICAL
        && grd[x][y] <= DNGN_UNDISCOVERED_TRAP);
}

// Does the equivalent of KILL_RESET on all monsters in LOS. Should only be
// applied to new games.
void zap_los_monsters()
{
    losight(env.show, grd, you.x_pos, you.y_pos);

    for (int y = crawl_view.vlos1.y; y <= crawl_view.vlos2.y; ++y)
    {
        for (int x = crawl_view.vlos1.x; x <= crawl_view.vlos2.x; ++x)
        {
            if (!in_vlos(x, y))
                continue;

            const int gx = view2gridX(x),
                      gy = view2gridY(y);

            if (!map_bounds(gx, gy))
                continue;

            if (gx == you.x_pos && gy == you.y_pos)
                continue;

            int imon = mgrd[gx][gy];
            
            // at tutorial beginning disallow items in line of sight
            if (Options.tutorial_events[TUT_SEEN_FIRST_OBJECT])
            {
                int item = igrd[gx][gy];
            
                if (item != NON_ITEM && is_valid_item(mitm[item]) )
                    destroy_item(item);
            }

            if (imon == NON_MONSTER || imon == MHITYOU)
                continue;

            // If we ever allow starting with a friendly monster,
            // we'll have to check here.
            monsters *mon = &menv[imon];

            if (mons_class_flag( mon->type, M_NO_EXP_GAIN ))
                continue;
            
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Dismissing %s",
                 mon->name(DESC_PLAIN, true).c_str() );
#endif
            monster_die(mon, KILL_DISMISSED, 0);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// coord_def
int coord_def::distance_from(const coord_def &other) const
{
    return (grid_distance(x, y, other.x, other.y));
}

int random_rod_subtype()
{
    return STAFF_SMITING + random2(NUM_STAVES - STAFF_SMITING);
}
