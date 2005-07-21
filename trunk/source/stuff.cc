/*
 *  File:       stuff.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *   <4>    11/14/99     cdl    added random40(), made arg to random*() signed
 *   <3>    11/06/99     cdl    added random22()
 *   <2>     9/25/99     cdl    linuxlib -> liblinux
 *   <1>     -/--/--     LRH    Created
 */

#include "AppHdr.h"
#include "stuff.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// may need this later for something else {dlb}:
// required for table_lookup() {dlb}
//#include <stdarg.h>
// required for table_lookup() {dlb}

#ifdef DOS
#include <conio.h>
#endif

#ifdef LINUX
#include "liblinux.h"
#endif

#include "externs.h"

#include "macro.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "output.h"
#include "skills2.h"
#include "view.h"


// required for stuff::coinflip() and cf_setseed()
unsigned long cfseed;

// unfortunately required for near_stairs(ugh!):
extern unsigned char (*mapch) (unsigned char);

// Crude, but functional.
char *const make_time_string( time_t abs_time, char *const buff, int buff_size )
{
    const int days  = abs_time / 86400;
    const int hours = (abs_time % 86400) / 3600;
    const int mins  = (abs_time % 3600) / 60;
    const int secs  = abs_time % 60;

    char day_buff[32];

    if (days > 0)
    {
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

            if (monster_habitat(fmenv->type) != DNGN_FLOOR)
                continue;

            if (fmenv->speed_increment < 50)
                continue;

            // only friendly monsters,  or those actively seeking the
            // player,  will follow up/down stairs.
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
#ifdef LINUX
    lincurses_shutdown();
#endif

#ifdef MAC
    deinit_mac();
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

    new_level();

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
        strcpy(info, "Something appears ");
        strcat(info, (you.species == SP_NAGA || you.species == SP_CENTAUR)
                                            ? "before you" : "at your feet");
        strcat(info, "!");
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
bool yesno( const char *str, bool safe, bool clear_after )
{
    unsigned char tmp;

    for (;;)
    {
        mpr(str, MSGCH_PROMPT);

        tmp = (unsigned char) getch();

        if (Options.easy_confirm == CONFIRM_ALL_EASY
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

// More accurate than distance() given the actual movement geonmetry -- bwr
int grid_distance( int x, int y, int x2, int y2 )
{
    const int dx = abs( x - x2 );
    const int dy = abs( y - y2 );

    // returns distance in terms of moves:
    return ((dx > dy) ? dx : dy);
}

int distance( int x, int y, int x2, int y2 )
{
    //jmf: now accurate, but remember to only compare vs. pre-squared distances.
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
#ifdef USE_SILENCE_CODE

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

#else
    return false;
#endif
}                               // end silenced()

bool player_can_hear(char x, char y)
{
#ifdef USE_SILENCE_CODE
    return (!silenced(x, y) && !silenced(you.x_pos, you.y_pos));
#else
    return true;
#endif
}                               // end player_can_hear()

unsigned char random_colour(void)
{
    return (1 + random2(15));
}                               // end random_colour()

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
                stair_gfx = mapch(grd[x][y]);
                return ((x == you.x_pos && y == you.y_pos) ? 2 : 1);
            }
        }
    }

    return false;
}
