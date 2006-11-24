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
#include "direct.h"
#include "monplace.h"
#include "stuff.h"
#include "view.h"

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


// may need this later for something else {dlb}:
// required for table_lookup() {dlb}
//#include <stdarg.h>
// required for table_lookup() {dlb}

#ifdef DOS
#include <conio.h>
#endif

#ifdef UNIX
#include "libunix.h"
#endif

#include "delay.h"
#include "externs.h"

#include "macro.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mt19937ar.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "skills2.h"
#include "view.h"


// required for stuff::coinflip() and cf_setseed()
unsigned long cfseed;

// unfortunately required for near_stairs(ugh!):
extern unsigned char (*mapch) (unsigned char);

// Crude, but functional.
char *const make_time_string( time_t abs_time, char *const buff, int buff_size,
                              bool terse )
{
    const int days  = abs_time / 86400;
    const int hours = (abs_time % 86400) / 3600;
    const int mins  = (abs_time % 3600) / 60;
    const int secs  = abs_time % 60;

    char day_buff[32];

    if (days > 0)
    {
        if (terse)
            snprintf(day_buff, sizeof day_buff, "%d, ", days);
        else
            snprintf( day_buff, sizeof(day_buff), "%d day%s, ", 
                  days, (days > 1) ? "s" : "" );
    }

    snprintf( buff, buff_size, "%s%02d:%02d:%02d", 
              (days > 0) ? day_buff : "", hours, mins, secs );

    return (buff);
}

void set_redraw_status( unsigned long flags )
{
    you.redraw_status_flags |= flags;
}

void tag_followers( void )
{   
    int count_x, count_y;

    for (count_x = you.x_pos - 1; count_x <= you.x_pos + 1; count_x++)
    {   
        for (count_y = you.y_pos - 1; count_y <= you.y_pos + 1; count_y++)
        {   
            if (count_x == you.x_pos && count_y == you.y_pos)
                continue;

            if (mgrd[count_x][count_y] == NON_MONSTER)
                continue;

            struct monsters *fmenv = &menv[mgrd[count_x][count_y]];

            if ((fmenv->type == MONS_PANDEMONIUM_DEMON)
                || (fmenv->type == MONS_PLANT)
                || (fmenv->type == MONS_FUNGUS)
                || (fmenv->type == MONS_OKLOB_PLANT)
                || (fmenv->type == MONS_CURSE_SKULL)
                || (fmenv->type == MONS_PLAYER_GHOST)  // cdl
                || (fmenv->type == MONS_CURSE_TOE)
                || (fmenv->type == MONS_POTION_MIMIC)
                || (fmenv->type == MONS_WEAPON_MIMIC)
                || (fmenv->type == MONS_ARMOUR_MIMIC)
                || (fmenv->type == MONS_SCROLL_MIMIC)
                || (fmenv->type == MONS_GOLD_MIMIC)
                || (fmenv->type == -1))
            {   
                continue;
            }

            if (!monster_habitable_grid(fmenv, DNGN_FLOOR))
                continue;

            if (fmenv->speed_increment < 50)
                continue;

            // only friendly monsters, or those actively seeking the
            // player, will follow up/down stairs.
            if (!(mons_friendly(fmenv) ||
                (fmenv->behaviour == BEH_SEEK && fmenv->foe == MHITYOU)))
            {   
                continue;
            }

            // monster is chasing player through stairs:
            fmenv->flags |= MF_TAKING_STAIRS;

#if DEBUG_DIAGNOSTICS
            snprintf( info, INFO_SIZE, "%s is marked for following.",
                      ptr_monam( fmenv, DESC_CAP_THE ) );
            mpr( info, MSGCH_DIAGNOSTICS );
#endif  
        }
    }
}

void untag_followers( void )
{
    for (int m = 0; m < MAX_MONSTERS; m++)
    {
        struct monsters *mon = &menv[m];
        mon->flags &= (~MF_TAKING_STAIRS);
    }
}

unsigned char get_ch(void)
{
    unsigned char gotched = getch();

    if (gotched == 0)
        gotched = getch();

    return gotched;
}                               // end get_ch()

void seed_rng(long seed)
{
#ifdef USE_SYSTEM_RAND
    srand(seed);
#else
    // MT19937 -- see mt19937ar.cc for details/licence
    init_genrand(seed);
#endif
}

void seed_rng()
{
    unsigned long seed = time( NULL );
#ifdef USE_MORE_SECURE_SEED
    struct tms  buf;
    seed += times( &buf ) + getpid();
#endif

    seed_rng(seed);
#ifdef USE_SYSTEM_RAND
    cf_setseed();
#endif
}

#ifdef USE_SYSTEM_RAND
int random2(int max)
{
#ifdef USE_NEW_RANDOM
    //return (int) ((((float) max) * rand()) / RAND_MAX); - this is bad!
    // Uses FP, so is horribly slow on computers without coprocessors.
    // Taken from comp.lang.c FAQ. May have problems as max approaches
    // RAND_MAX, but this is rather unlikely.
    // We've used rand() rather than random() for the portability, I think.

    if (max < 1 || max >= RAND_MAX)
        return 0;
    else
        return (int) rand() / (RAND_MAX / max + 1);
#else

    if (max < 1)
        return 0;

    return rand() % max;
#endif
}

// required for stuff::coinflip()
#define IB1 1
#define IB2 2
#define IB5 16
#define IB18 131072
#define MASK (IB1 + IB2 + IB5)
// required for stuff::coinflip()

// I got to thinking a bit more about how much people talk
// about RNGs and RLs and also about the issue of performance
// when it comes to Crawl's RNG ... turning to *Numerical
// Recipies in C* (Chapter 7-4, page 298), I hit upon what
// struck me as a fine solution.

// You can read all the details about this function (pretty
// much stolen shamelessly from NRinC) elsewhere, but having
// tested it out myself I think it satisfies Crawl's incessant
// need to decide things on a 50-50 flip of the coin. No call
// to random2() required -- along with all that wonderful math
// and type casting -- and only a single variable its pointer,
// and some bitwise operations to randomly generate 1s and 0s!
// No parameter passing, nothing. Too good to be true, but it
// works as long as cfseed is not set to absolute zero when it
// is initialized ... good for 2**n-1 random bits before the
// pattern repeats (n = long's bitlength on your platform).
// It also avoids problems with poor implementations of rand()
// on some platforms in regards to low-order bits ... a big
// problem if one is only looking for a 1 or a 0 with random2()!

// Talk about a hard sell! Anyway, it returns bool, so please
// use appropriately -- I set it to bool to prevent such
// tomfoolery, as I think that pure RNG and quickly grabbing
// either a value of 1 or 0 should be separated where possible
// to lower overhead in Crawl ... at least until it assembles
// itself into something a bit more orderly :P 16jan2000 {dlb}

// NB(1): cfseed is defined atop stuff.cc
// NB(2): IB(foo) and MASK are defined somewhere in defines.h
// NB(3): the function assumes that cf_setseed() has been called
//        beforehand - the call is presently made in acr::initialise()
//        right after srandom() and srand() are called (note also
//        that cf_setseed() requires rand() - random2 returns int
//        but a long can't hurt there).
bool coinflip(void)
{
    extern unsigned long cfseed;        // defined atop stuff.cc
    unsigned long *ptr_cfseed = &cfseed;

    if (*ptr_cfseed & IB18)
    {
        *ptr_cfseed = ((*ptr_cfseed ^ MASK) << 1) | IB1;
        return true;
    }
    else
    {
        *ptr_cfseed <<= 1;
        return false;
    }
}                               // end coinflip()

// cf_setseed should only be called but once in all of Crawl!!! {dlb}
void cf_setseed(void)
{
    extern unsigned long cfseed;        // defined atop stuff.cc
    unsigned long *ptr_cfseed = &cfseed;

    do
    {
        // using rand() here makes these predictable -- bwr
        *ptr_cfseed = rand();
    }
    while (*ptr_cfseed == 0);
}

static std::stack<long> rng_states;
void push_rng_state()
{
    // XXX: Does this even work? randart.cc uses it, but I can't find anything
    // that says this will restore the RNG to its original state. Anyway, we're
    // now using MT with a deterministic push/pop.
    rng_states.push(rand());
}

void pop_rng_state()
{
    if (!rng_states.empty())
    {
        seed_rng(rng_states.top());
        rng_states.pop();
    }
}

unsigned long random_int( void )
{
    return rand();
}

#else   // USE_SYSTEM_RAND

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

#endif  // USE_SYSTEM_RAND

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

// answers the question: "Is a grid within character's line of sight?"
bool see_grid(unsigned char grx, unsigned char gry)
{
    if (grx > you.x_pos - 9 && grx < you.x_pos + 9
                        && gry > you.y_pos - 9 && gry < you.y_pos + 9)
    {
        if (env.show[grx - you.x_pos + 9][gry - you.y_pos + 9] != 0)
            return true;

        // rare case: can player see self?  (of course!)
        if (grx == you.x_pos && gry == you.y_pos)
            return true;
    }

    return false;
}                               // end see_grid()

void end(int end_arg)
{
#ifdef UNIX
    unixcurses_shutdown();
#endif

#ifdef WIN32CONSOLE
    deinit_libw32c();
#endif

    exit(end_arg);
}

void redraw_screen(void)
{
#ifdef PLAIN_TERM
// this function is used for systems without gettext/puttext to redraw the
// playing screen after a call to for example inventory.
    draw_border();

    you.redraw_hit_points = 1;
    you.redraw_magic_points = 1;
    you.redraw_strength = 1;
    you.redraw_intelligence = 1;
    you.redraw_dexterity = 1;
    you.redraw_armour_class = 1;
    you.redraw_evasion = 1;
    you.redraw_gold = 1;
    you.redraw_experience = 1;
    you.wield_change = true;

    set_redraw_status( REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK );

    print_stats();

    if (Options.delay_message_clear)
        mesclr( true );

    bool note_status = notes_are_active();
    activate_notes(false);
    new_level();
    activate_notes(note_status);

    viewwindow(1, false);
#endif
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

// I got so tired of seeing: ".. && random2(foo) == 0 && .." in the code
// that I broke down and wrote this little -- very little -- function.
// anyway, I think improving the readability of the code offsets whatever
// overhead the additional (intermediary) function call added to Crawl -
// we'll just make it up by tightening code elsewhere, right guys?
// [use with == and != only .. never directly with comparisons or math]
//                                                      -- 14jan2000 {dlb}
bool one_chance_in(int a_million)
{
    return (random2(a_million) == 0);
}                               // end one_chance_in() - that's it? :P {dlb}


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

void canned_msg(unsigned char which_message)
{
    switch (which_message)
    {
    case MSG_SOMETHING_APPEARS:
        snprintf(info, INFO_SIZE, "Something appears %s!",
                 (you.species == SP_NAGA || you.species == SP_CENTAUR)
                 ? "before you" : "at your feet");
        mpr(info);
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
bool yesno( const char *str, bool safe, int safeanswer, bool clear_after )
{
    unsigned char tmp;

    interrupt_activity( AI_FORCE_INTERRUPT );
    for (;;)
    {
        mpr(str, MSGCH_PROMPT);

        tmp = (unsigned char) getch();

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
        else
            mpr("[Y]es or [N]o only, please.");
    }
}                               // end yesno()

// like yesno(), but returns 0 for no, 1 for yes, and -1 for quit
int yesnoquit( const char* str, bool safe, int safeanswer, bool clear_after )
{
    unsigned char tmp;

    interrupt_activity( AI_FORCE_INTERRUPT );
    while (1)
    {
        mpr(str, MSGCH_PROMPT);

        tmp = (unsigned char) getch();

	if ( tmp == 27 || tmp == 'q' || tmp == 'Q' )
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

bool silenced(char x, char y)
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

bool player_can_hear(char x, char y)
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
void random_in_bounds( int &x_pos, int &y_pos, int terr, bool empty, bool excl )
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
        else if (terr == DNGN_DEEP_WATER && grd[x_pos][y_pos] == DNGN_SHALLOW_WATER)
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

// takes rectangle (x1,y1)-(x2,y2) and shifts it somewhere randomly in bounds
void random_place_rectangle( int &x1, int &y1, int &x2, int &y2, bool excl )
{
    const unsigned int dx = abs( x2 - x1 );
    const unsigned int dy = abs( y2 - y1 );

    x1 = X_BOUND_1 + random2( X_WIDTH - dx - 2 * excl ) + excl;
    y1 = Y_BOUND_1 + random2( Y_WIDTH - dy - 2 * excl ) + excl;

    x2 = x1 + dx;
    y2 = y1 + dy;
}

// returns true if point (px,py) is in rectangle (rx1, ry1) - (rx2, ry2)
bool in_rectangle( int px, int py, int rx1, int ry1, int rx2, int ry2, 
                   bool excl )
{
    ASSERT( rx1 < rx2 - 1 && ry1 < ry2 - 1 );

    if (excl)
    {
        rx1++;
        rx2--;
        ry1++;
        ry2--;
    }

    return (px >= rx1 && px <= rx2 && py >= ry1 && py <= ry2);
}

// XXX: this can be done better
// returns true if rectables a and b overlap
bool rectangles_overlap( int ax1, int ay1, int ax2, int ay2,
                         int bx1, int by1, int bx2, int by2,
                         bool excl )
{
    ASSERT( ax1 < ax2 - 1 && ay1 < ay2 - 1 );
    ASSERT( bx1 < bx2 - 1 && by1 < by2 - 1 );

    if (excl)
    {
        ax1++;
        ax2--;
        ay1++;
        ay2--;
    }

    return (in_rectangle( ax1, ay1, bx1, by1, bx2, by2, excl )
            || in_rectangle( ax1, ay2, bx1, by1, bx2, by2, excl )
            || in_rectangle( ax2, ay1, bx1, by1, bx2, by2, excl )
            || in_rectangle( ax2, ay2, bx1, by1, bx2, by2, excl ));
}

unsigned char random_colour(void)
{
    return (1 + random2(15));
}                               // end random_colour()

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
    // which might want a consistant result.
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

// returns 0 if the point is not near stairs
// returns 1 if the point is near unoccupied stairs
// returns 2 if the point is near player-occupied stairs

int near_stairs(int px, int py, int max_dist, unsigned char &stair_gfx)
{
    int i,j;

    for(i=-max_dist; i<=max_dist; i++)
    {
        for(j=-max_dist; j<=max_dist; j++)
        {
            int x = px + i;
            int y = py + j;

            if (x<0 || x>=GXM || y<0 || y>=GYM)
                continue;

            // very simple check
            if (grd[x][y] >= DNGN_STONE_STAIRS_DOWN_I
                && grd[x][y] <= DNGN_RETURN_FROM_SWAMP
                && grd[x][y] != DNGN_ENTER_SHOP)        // silly
            {
                stair_gfx = get_sightmap_char(grd[x][y]);
                return ((x == you.x_pos && y == you.y_pos) ? 2 : 1);
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

    for (int y = LOS_SY; y <= LOS_EY; ++y)
    {
        for (int x = LOS_SX; x <= LOS_EX; ++x)
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
            if (imon == NON_MONSTER || imon == MHITYOU)
                continue;

            // If we ever allow starting with a friendly monster,
            // we'll have to check here.
            monsters *mon = &menv[imon];
#ifdef DEBUG_DIAGNOSTICS
            char mname[ITEMNAME_SIZE];
            moname(mon->type, true, DESC_PLAIN, mname);
            mprf(MSGCH_DIAGNOSTICS, "Dismissing %s", mname);
#endif
            monster_die(mon, KILL_DISMISSED, 0);
        }
    }
}
