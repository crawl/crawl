/*
 *  File:       stuff.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "cio.h"
#include "database.h"
#include "directn.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "state.h"
#include "stuff.h"
#include "view.h"

#include <cstdarg>
#include <sstream>
#include <iomanip>
#include <algorithm>

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
 #ifndef USE_TILE
 #include "libunix.h"
 #endif
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
#include "religion.h"
#include "skills2.h"
#include "tutorial.h"
#include "view.h"

stack_iterator::stack_iterator(const coord_def& pos)
{
    cur_link = igrd(pos);
    if ( cur_link != NON_ITEM )
        next_link = mitm[cur_link].link;
    else
        next_link = NON_ITEM;
}

stack_iterator::stack_iterator(int start_link)
{
    cur_link = start_link;
    if ( cur_link != NON_ITEM )
        next_link = mitm[cur_link].link;
    else
        next_link = NON_ITEM;
}

stack_iterator::operator bool() const
{
    return ( cur_link != NON_ITEM );
}

item_def& stack_iterator::operator*() const
{
    ASSERT( cur_link != NON_ITEM );
    return mitm[cur_link];
}

item_def* stack_iterator::operator->() const
{
    ASSERT( cur_link != NON_ITEM );
    return &mitm[cur_link];
}

int stack_iterator::link() const
{
    return cur_link;
}

const stack_iterator& stack_iterator::operator ++ ()
{
    cur_link = next_link;
    if ( cur_link != NON_ITEM )
        next_link = mitm[cur_link].link;
    return *this;
}

stack_iterator stack_iterator::operator++(int dummy)
{
    const stack_iterator copy = *this;
    ++(*this);
    return copy;
}

rectangle_iterator::rectangle_iterator( const coord_def& corner1,
                                        const coord_def& corner2 )
{
    topleft.x = std::min(corner1.x, corner2.x);
    topleft.y = std::min(corner1.y, corner2.y); // not really necessary
    bottomright.x = std::max(corner1.x, corner2.x);
    bottomright.y = std::max(corner1.y, corner2.y);
    current = topleft;
}

rectangle_iterator::rectangle_iterator( int x_border_dist, int y_border_dist )
{
    if (y_border_dist < 0)
        y_border_dist = x_border_dist;

    topleft.set( x_border_dist, y_border_dist );
    bottomright.set( GXM - x_border_dist - 1, GYM - y_border_dist - 1 );
    current = topleft;
}

rectangle_iterator::operator bool() const
{
    return !(current.y > bottomright.y);
}

coord_def rectangle_iterator::operator *() const
{
    return current;
}

const coord_def* rectangle_iterator::operator->() const
{
    return &current;
}

rectangle_iterator& rectangle_iterator::operator ++()
{
    if ( current.x == bottomright.x )
    {
        current.x = topleft.x;
        current.y++;
    }
    else
    {
        current.x++;
    }
    return *this;
}

rectangle_iterator rectangle_iterator::operator++( int dummy )
{
    const rectangle_iterator copy = *this;
    ++(*this);
    return (copy);
}

radius_iterator::radius_iterator(const coord_def& _center, int _radius,
                                 bool _roguelike_metric, bool _require_los,
                                 bool _exclude_center,
                                 const env_show_grid* _losgrid)
    : center(_center), radius(_radius), roguelike_metric(_roguelike_metric),
      require_los(_require_los), exclude_center(_exclude_center),
      losgrid(_losgrid), iter_done(false)
{
    reset();
}

void radius_iterator::reset()
{
    iter_done = false;

    location.x = center.x - radius;
    location.y = center.y - radius;

    if ( !this->on_valid_square() )
        ++(*this);
}

bool radius_iterator::done() const
{
    return iter_done;
}

coord_def radius_iterator::operator *() const
{
    return location;
}

const coord_def* radius_iterator::operator->() const
{
    return &location;
}

void radius_iterator::step()
{
    const int minx = std::max(X_BOUND_1+1, center.x - radius);
    const int maxx = std::min(X_BOUND_2-1, center.x + radius);
    const int maxy = std::min(Y_BOUND_2-1, center.y + radius);

    // Sweep L-R, U-D
    location.x++;
    if (location.x > maxx)
    {
        location.x = minx;
        location.y++;
        if (location.y > maxy)
            iter_done = true;
    }
}

void radius_iterator::step_back()
{
    const int minx = std::max(X_BOUND_1+1, center.x - radius);
    const int maxx = std::min(X_BOUND_2-1, center.x + radius);
    const int miny = std::max(Y_BOUND_1+1, center.y - radius);

    location.x--;
    if ( location.x < minx )
    {
        location.x = maxx;
        location.y--;
        if ( location.y < miny )
            iter_done = true;   // hmm
    }
}

bool radius_iterator::on_valid_square() const
{
    if (!in_bounds(location))
        return (false);
    if (!roguelike_metric && (location - center).abs() > radius*radius)
        return (false);
    if (require_los)
    {
        if (!losgrid && !see_grid(location))
            return (false);
        if (losgrid && !see_grid(*losgrid, center, location))
            return (false);
    }
    if (exclude_center && location == center)
        return (false);

    return (true);
}

const radius_iterator& radius_iterator::operator++()
{
    do
        this->step();
    while (!this->done() && !this->on_valid_square());

    return (*this);
}

const radius_iterator& radius_iterator::operator--()
{
    do
        this->step_back();
    while (!this->done() && !this->on_valid_square());

    return (*this);
}

radius_iterator radius_iterator::operator++(int dummy)
{
    const radius_iterator copy = *this;
    ++(*this);
    return (copy);
}

radius_iterator radius_iterator::operator--(int dummy)
{
    const radius_iterator copy = *this;
    --(*this);
    return (copy);
}

// Crude, but functional.
std::string make_time_string(time_t abs_time, bool terse)
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

void set_redraw_status(unsigned long flags)
{
    you.redraw_status_flags |= flags;
}

static bool _tag_follower_at(const coord_def &pos)
{
    if (!in_bounds(pos) || pos == you.pos())
        return (false);

    monsters *fmenv = monster_at(pos);
    if (fmenv == NULL)
        return (false);

    if (fmenv->type == MONS_PLAYER_GHOST
        || !fmenv->alive()
        || fmenv->incapacitated()
        || !mons_can_use_stairs(fmenv)
        || mons_is_stationary(fmenv))
    {
        return (false);
    }

    if (!monster_habitable_grid(fmenv, DNGN_FLOOR))
        return (false);

    if (fmenv->speed_increment < 50)
        return (false);

    // Only friendly monsters, or those actively seeking the
    // player, will follow up/down stairs.
    if (!mons_friendly(fmenv)
        && (!mons_is_seeking(fmenv) || fmenv->foe != MHITYOU))
    {
        return (false);
    }

    // Monsters that are not directly adjacent are subject to more
    // stringent checks.
    if ((pos - you.pos()).abs() > 2)
    {
        if (!mons_friendly(fmenv))
            return (false);

        // Undead will follow Yredelemnul worshippers, and orcs will
        // follow Beogh worshippers.
        if ((you.religion != GOD_YREDELEMNUL && you.religion != GOD_BEOGH)
            || !is_follower(fmenv))
        {
            return (false);
        }
    }

    // Monster is chasing player through stairs.
    fmenv->flags |= MF_TAKING_STAIRS;

    // Clear patrolling/travel markers.
    fmenv->patrol_point.reset();
    fmenv->travel_path.clear();
    fmenv->travel_target = MTRAV_NONE;

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
    for (adjacent_iterator ai; ai; ++ai)
        if (const monsters *mon = monster_at(*ai))
            if (!mons_friendly(mon))
                return (2);

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
                    if (_tag_follower_at(fp))
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
    for (int m = 0; m < MAX_MONSTERS; ++m)
        menv[m].flags &= (~MF_TAKING_STAIRS);
}

unsigned char get_ch()
{
    unsigned char gotched = getch();

    if (gotched == 0)
        gotched = getch();

    return gotched;
}

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

// Chooses one of the numbers passed in at random. The list of numbers
// must be terminated with -1.
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

// Chooses one of the strings passed in at random. The list of strings
// must be terminated with NULL.
const char* random_choose_string(const char* first, ...)
{
    va_list args;
    va_start(args, first);

    const char* chosen = first;
    int count = 1, nargs = 100;

    while (nargs-- > 0)
    {
        char* pick = va_arg(args, char*);
        if (pick == NULL)
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

int random2(int max)
{
    if (max <= 1)
        return (0);

    return (static_cast<int>(genrand_int32() / (0xFFFFFFFFUL / max + 1)));
}

bool coinflip(void)
{
    return (static_cast<bool>(random2(2)));
}

// Returns random2(x) if random_factor is true, otherwise the mean.
int maybe_random2(int x, bool random_factor)
{
    if (random_factor)
        return (random2(x));
    else
        return (x / 2);
}

void push_rng_state()
{
    push_mt_state();
}

void pop_rng_state()
{
    pop_mt_state();
}

// Attempts to make missile weapons nicer to the player by reducing the
// extreme variance in damage done.
void scale_dice(dice_def &dice, int threshold)
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

// random2avg() returns same mean value as random2() but with a lower variance
// never use with rolls < 2 as that would be silly - use random2() instead {dlb}
int random2avg(int max, int rolls)
{
    int sum = random2(max);

    for (int i = 0; i < (rolls - 1); i++)
        sum += random2(max + 1);

    return (sum / rolls);
}

int roll_dice(int num, int size)
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
            ret += random2(size);
    }

    return (ret);
}

// originally designed to randomize evasion -
// values are slightly lowered near (max) and
// approach an upper limit somewhere near (limit/2)
int random2limit(int max, int limit)
{
    int i;
    int sum = 0;

    if (max < 1)
        return (0);

    for (i = 0; i < max; i++)
    {
        if (random2(limit) >= i)
            sum++;
    }

    return (sum);
}

void cio_init()
{
    crawl_state.io_inited = true;

#if defined(UNIX) && !defined(USE_TILE)
    unixcurses_startup();
#endif

#ifdef WIN32CONSOLE
    init_libw32c();
#endif

#ifdef DOS
    init_libdos();
#endif

    crawl_view.init_geometry();

#ifdef USE_TILE
    tiles.resize();
#endif

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

#if defined(USE_TILE)
    tiles.shutdown();
#elif defined(UNIX)
    unixcurses_shutdown();
#endif

#ifdef WIN32CONSOLE
    deinit_libw32c();
#endif

    msg::deinitialise_mpr_streams();

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
    if (exit_code && !crawl_state.arena)
    {
        fprintf(stderr, "Hit Enter to continue...\n");
        getchar();
    }
#endif

    exit(exit_code);
}

void redraw_screen(void)
{
    if (!crawl_state.need_save)
    {
        // If the game hasn't started, don't do much.
        clrscr();
        return;
    }

    draw_border();

    you.redraw_hit_points   = true;
    you.redraw_magic_points = true;
    you.redraw_strength     = true;
    you.redraw_intelligence = true;
    you.redraw_dexterity    = true;
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_experience   = true;
    you.wield_change        = true;
    you.redraw_quiver       = true;

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

    viewwindow(true, false);
}

// STEPDOWN FUNCTION to replace conditional chains in spells2.cc 12jan2000 {dlb}
// it is a bit more extensible and optimizes the logical structure, as well
// usage: cast_summon_swarm() cast_summon_wraiths() cast_summon_scorpions()
//        cast_summon_horrible_things()
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

// I hope this permits easier/more experimentation with value stepdowns
// in the code.  It really needs to be rewritten to accept arbitrary
// (unevenly spaced) steppings.
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

}

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
int div_rand_round(int num, int den)
{
    return (num / den + (random2(den) < num % den));
}

int div_round_up(int num, int den)
{
    return (num / den + (num % den != 0));
}

bool one_chance_in(int a_million)
{
    return (random2(a_million) == 0);
}

bool x_chance_in_y(int x, int y)
{
    if (x <= 0)
        return (false);

    if (x >= y)
        return (true);

    return (random2(y) < x);
}

// Simple little function to quickly modify all three stats
// at once - does check for '0' modifiers to prevent needless
// adding .. could use checking for sums less than zero, I guess.
// Used in conjunction with newgame::species_stat_init() and
// newgame::job_stat_init() routines 24jan2000. {dlb}
void modify_all_stats(int STmod, int IQmod, int DXmod)
{
    if (STmod)
    {
        you.strength     += STmod;
        you.max_strength += STmod;
        you.redraw_strength = true;
    }

    if (IQmod)
    {
        you.intel     += IQmod;
        you.max_intel += IQmod;
        you.redraw_intelligence = true;
    }

    if (DXmod)
    {
        you.dex     += DXmod;
        you.max_dex += DXmod;
        you.redraw_dexterity = true;
    }
}

void canned_msg(canned_message_type which_message)
{
    switch (which_message)
    {
    case MSG_SOMETHING_APPEARS:
        mprf("Something appears %s!",
             (you.species == SP_NAGA || player_mutation_level(MUT_HOOVES))
                 ? "before you" : "at your feet");
        break;
    case MSG_NOTHING_HAPPENS:
        mpr("Nothing appears to happen.");
        break;
    case MSG_YOU_RESIST:
        mpr("You resist.");
        learned_something_new(TUT_YOU_RESIST);
        break;
    case MSG_YOU_PARTIALLY_RESIST:
        mpr("You partially resist.");
        break;
    case MSG_TOO_BERSERK:
        mpr("You are too berserk!");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_PRESENT_FORM:
        mpr("You can't do that in your present form.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NOTHING_CARRIED:
        mpr("You aren't carrying anything.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_CANNOT_DO_YET:
        mpr("You can't do that yet.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_OK:
        mpr("Okay, then.", MSGCH_PROMPT);
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_UNTHINKING_ACT:
        mpr("Why would you want to do that?");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_SPELL_FIZZLES:
        mpr("The spell fizzles.");
        break;
    case MSG_HUH:
        mpr("Huh?", MSGCH_EXAMINE_FILTER);
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_EMPTY_HANDED:
        mpr("You are now empty-handed.");
        break;
    }
}

// Like yesno, but requires a full typed answer.
// Unlike yesno, prompt should have no trailing space.
// Returns true if the user typed "yes", false if something else or cancel.
bool yes_or_no( const char* fmt, ... )
{
    char buf[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    buf[sizeof(buf)-1] = 0;

    mprf(MSGCH_PROMPT, "%s? (Confirm with \"yes\".) ", buf);

    if (cancelable_get_line(buf, sizeof buf))
        return (false);
    if (strcasecmp(buf, "yes") != 0)
        return (false);

    return (true);
}

// jmf: general helper (should be used all over in code)
//      -- idea borrowed from Nethack
bool yesno( const char *str, bool safe, int safeanswer, bool clear_after,
            bool interrupt_delays, bool noprompt,
            const explicit_keymap *map )
{
    if (interrupt_delays && !crawl_state.is_repeating_cmd())
        interrupt_activity( AI_FORCE_INTERRUPT );

    std::string prompt = make_stringf("%s ", str ? str : "Buggy prompt?");

    while (true)
    {
        if (!noprompt)
            mpr(prompt.c_str(), MSGCH_PROMPT);

        int tmp = getchm(KC_CONFIRM);

        if (map && map->find(tmp) != map->end())
            tmp = map->find(tmp)->second;

        if (safeanswer
            && (tmp == ' ' || tmp == ESCAPE || tmp == CONTROL('G')
                || tmp == '\r' || tmp == '\n'))
        {
            tmp = safeanswer;
        }

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || Options.easy_confirm == CONFIRM_SAFE_EASY && safe)
        {
            tmp = toupper( tmp );
        }

        if (clear_after)
            mesclr();

        if (tmp == 'N')
            return (false);
        else if (tmp == 'Y')
            return (true);
        else if (!noprompt)
            mpr("[Y]es or [N]o only, please.");
    }
}

static std::string _list_alternative_yes(char yes1, char yes2,
                                         bool lowered = false,
                                         bool brackets = false)
{
    std::string help = "";
    bool print_yes = false;
    if (yes1 != 'Y')
    {
        if (lowered)
            help += tolower(yes1);
        else
            help += yes1;
        print_yes = true;
    }

    if (yes2 != 'Y' && yes2 != yes1)
    {
        if (print_yes)
            help += "/";

        if (lowered)
            help += tolower(yes2);
        else
            help += yes2;
        print_yes = true;
    }

    if (print_yes)
    {
        if (brackets)
            help = " (" + help + ")";
        else
            help = "/" + help;
    }

    return help;
}

static std::string _list_allowed_keys(char yes1, char yes2,
                                      bool lowered = false,
                                      bool allow_all = false)
{
    std::string result = " [";
                result += (lowered ? "y" : "Y");
                result += _list_alternative_yes(yes1, yes2, lowered);
                if (allow_all)
                    result += (lowered? "/a" : "/A");
                result += (lowered ? "/n/q" : "/N/Q");
                result += "]";

    return (result);
}

// Like yesno(), but returns 0 for no, 1 for yes, and -1 for quit.
// alt_yes and alt_yes2 allow up to two synonyms for 'Y'.
// FIXME: This function is shaping up to be a monster. Help!
int yesnoquit( const char* str, bool safe, int safeanswer, bool allow_all,
               bool clear_after, char alt_yes, char alt_yes2 )
{
    if (!crawl_state.is_repeating_cmd())
        interrupt_activity( AI_FORCE_INTERRUPT );

    std::string prompt =
        make_stringf("%s%s ", str ? str : "Buggy prompt?",
                     _list_allowed_keys(alt_yes, alt_yes2,
                                        safe, allow_all).c_str());
    while (true)
    {
        mpr(prompt.c_str(), MSGCH_PROMPT);

        int tmp = getchm(KC_CONFIRM);

        if (tmp == CK_ESCAPE || tmp == CONTROL('G') || tmp == 'q' || tmp == 'Q')
            return -1;

        if ((tmp == ' ' || tmp == '\r' || tmp == '\n') && safeanswer)
            tmp = safeanswer;

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || safe && Options.easy_confirm == CONFIRM_SAFE_EASY)
        {
            tmp = toupper( tmp );
        }

        if (clear_after)
            mesclr();

        if (tmp == 'N')
            return 0;
        else if (tmp == 'Y' || tmp == alt_yes || tmp == alt_yes2)
            return 1;
        else if (allow_all)
        {
            if (tmp == 'A')
                return 2;
            else
                mprf("Choose [Y]es%s, [N]o, [Q]uit, or [A]ll!",
                     _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
        }
        else
        {
            mprf("[Y]es%s, [N]o or [Q]uit only, please.",
                 _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
        }
    }
}

int grid_distance( const coord_def& p1, const coord_def& p2 )
{
    return grid_distance(p1.x, p1.y, p2.x, p2.y);
}

// More accurate than distance() given the actual movement geometry -- bwr
int grid_distance( int x, int y, int x2, int y2 )
{
    const int dx = abs( x - x2 );
    const int dy = abs( y - y2 );

    // returns distance in terms of moves:
    return ((dx > dy) ? dx : dy);
}

int distance( const coord_def& p1, const coord_def& p2 )
{
    return distance(p1.x, p1.y, p2.x, p2.y);
}

int distance( int x, int y, int x2, int y2 )
{
    //jmf: now accurate, but remember to only compare vs. pre-squared distances
    //     thus, next to == (distance(m1.x,m1.y, m2.x,m2.y) <= 2)
    const int dx = x - x2;
    const int dy = y - y2;

    return ((dx * dx) + (dy * dy));
}

bool adjacent( const coord_def& p1, const coord_def& p2 )
{
    return grid_distance(p1, p2) <= 1;
}

bool silenced(const coord_def& p)
{
    // FIXME: implement for monsters
    return (you.duration[DUR_SILENCE] && distance(p, you.pos()) <= 6*6 + 1);
}

bool player_can_hear(const coord_def& p)
{
    return (!silenced(p) && !silenced(you.pos()));
}

bool in_bounds_x(int x)
{
    return (x > X_BOUND_1 && x < X_BOUND_2);
}

bool in_bounds_y(int y)
{
    return (y > Y_BOUND_1 && y < Y_BOUND_2);
}

// Returns true if inside the area the player can move and dig (ie exclusive).
bool in_bounds(int x, int y)
{
    return (in_bounds_x(x) && in_bounds_y(y));
}

bool map_bounds_x(int x)
{
    return (x >= X_BOUND_1 && x <= X_BOUND_2);
}

bool map_bounds_y(int y)
{
    return (y >= Y_BOUND_1 && y <= Y_BOUND_2);
}

// Returns true if inside the area the player can map (ie inclusive).
// Note that terrain features should be in_bounds() leaving an outer
// ring of rock to frame the level.
bool map_bounds(int x, int y)
{
    return (map_bounds_x(x) && map_bounds_y(y));
}

coord_def random_in_bounds()
{
    if (crawl_state.arena)
    {
        const coord_def &ul = crawl_view.glos1; // Upper left
        const coord_def &lr = crawl_view.glos2; // Lower right

        return coord_def(random_range(ul.x, lr.x - 1),
                         random_range(ul.y, lr.y - 1));
    }
    else
        return coord_def(random_range(MAPGEN_BORDER, GXM - MAPGEN_BORDER - 1),
                         random_range(MAPGEN_BORDER, GYM - MAPGEN_BORDER - 1));
}

unsigned char random_colour(void)
{
    return (1 + random2(15));
}

unsigned char random_uncommon_colour()
{
    unsigned char result;

    do
        result = random_colour();
    while (result == LIGHTCYAN || result == CYAN || result == BROWN);

    return (result);
}

// returns if a colour is one of the special element colours (ie not regular)
bool is_element_colour( int col )
{
    // stripping any COLFLAGS (just in case)
    return ((col & 0x007f) >= ETC_FIRE);
}

int element_colour( int element, bool no_random )
{
    // Doing this so that we don't have to do recursion here at all
    // (these were the only cases which had possible double evaluation):
    if (element == ETC_FLOOR)
        element = env.floor_colour;
    else if (element == ETC_ROCK)
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
    case ETC_FIRE:
        ret = (tmp_rand < 40) ? RED :
              (tmp_rand < 80) ? YELLOW
                              : LIGHTRED;
        break;

    case ETC_ICE:
        ret = (tmp_rand < 40) ? LIGHTBLUE :
              (tmp_rand < 80) ? BLUE
                              : WHITE;
        break;

    case ETC_EARTH:
        ret = (tmp_rand < 60) ? BROWN : LIGHTRED;
        break;

    case ETC_AIR:
        ret = (tmp_rand < 60) ? LIGHTGREY : WHITE;
        break;

    case ETC_ELECTRICITY:
        ret = (tmp_rand < 40) ? LIGHTCYAN :
              (tmp_rand < 80) ? LIGHTBLUE
                              : CYAN;
        break;

    case ETC_POISON:
        ret = (tmp_rand < 60) ? LIGHTGREEN : GREEN;
        break;

    case ETC_WATER:
        ret = (tmp_rand < 60) ? BLUE : CYAN;
        break;

    case ETC_MAGIC:
        ret = (tmp_rand < 30) ? LIGHTMAGENTA :
              (tmp_rand < 60) ? LIGHTBLUE :
              (tmp_rand < 90) ? MAGENTA
                              : BLUE;
        break;

    case ETC_MUTAGENIC:
    case ETC_WARP:
        ret = (tmp_rand < 60) ? LIGHTMAGENTA : MAGENTA;
        break;

    case ETC_ENCHANT:
        ret = (tmp_rand < 60) ? LIGHTBLUE : BLUE;
        break;

    case ETC_HEAL:
        ret = (tmp_rand < 60) ? LIGHTBLUE : YELLOW;
        break;

    case ETC_BLOOD:
        ret = (tmp_rand < 60) ? RED : DARKGREY;
        break;

    case ETC_DEATH:      // assassin
    case ETC_NECRO:      // necromancer
        ret = (tmp_rand < 80) ? DARKGREY : MAGENTA;
        break;

    case ETC_UNHOLY:     // ie demonology
        ret = (tmp_rand < 80) ? DARKGREY : RED;
        break;

    case ETC_DARK:
        ret = (tmp_rand < 80) ? DARKGREY : LIGHTGREY;
        break;

    case ETC_HOLY:
        ret = (tmp_rand < 60) ? YELLOW : WHITE;
        break;

    case ETC_VEHUMET:
        ret = (tmp_rand < 40) ? LIGHTRED :
              (tmp_rand < 80) ? LIGHTMAGENTA
                              : LIGHTBLUE;
        break;

    case ETC_BEOGH:
        ret = (tmp_rand < 60) ? LIGHTRED // plain Orc colour
                              : BROWN;   // Orcish mines wall/idol colour
        break;

    case ETC_CRYSTAL:
        ret = (tmp_rand < 40) ? LIGHTGREY :
              (tmp_rand < 80) ? GREEN
                              : LIGHTRED;
        break;

    case ETC_SLIME:
        ret = (tmp_rand < 40) ? GREEN :
              (tmp_rand < 80) ? BROWN
                              : LIGHTGREEN;
        break;

    case ETC_SMOKE:
        ret = (tmp_rand < 30) ? LIGHTGREY :
              (tmp_rand < 60) ? DARKGREY :
              (tmp_rand < 90) ? LIGHTBLUE
                              : MAGENTA;
        break;

    case ETC_JEWEL:
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

    case ETC_ELVEN:
        ret = (tmp_rand <  40) ? LIGHTGREEN :
              (tmp_rand <  80) ? GREEN :
              (tmp_rand < 100) ? LIGHTBLUE
                               : BLUE;
        break;

    case ETC_DWARVEN:
        ret = (tmp_rand <  40) ? BROWN :
              (tmp_rand <  80) ? LIGHTRED :
              (tmp_rand < 100) ? LIGHTGREY
                               : CYAN;
        break;

    case ETC_ORCISH:
        ret = (tmp_rand <  40) ? DARKGREY :
              (tmp_rand <  80) ? RED :
              (tmp_rand < 100) ? BROWN
                               : MAGENTA;
        break;

    case ETC_GILA:
        ret = (tmp_rand <  30) ? LIGHTMAGENTA :
              (tmp_rand <  60) ? MAGENTA :
              (tmp_rand <  90) ? YELLOW :
              (tmp_rand < 105) ? LIGHTRED
                               : RED;
        break;

    case ETC_STONE:
        if (player_in_branch( BRANCH_HALL_OF_ZOT ))
            ret = env.rock_colour;
        else
            ret = LIGHTGREY;
        break;

    case ETC_MIST:
        ret = tmp_rand < 100? CYAN : BLUE;
        break;

    case ETC_SHIMMER_BLUE:
        ret = random_choose_weighted(80, BLUE, 20, LIGHTBLUE, 5, CYAN, 0);
        break;

    case ETC_DECAY:
        ret = (tmp_rand < 60) ? BROWN : GREEN;
        break;

    case ETC_SILVER:
        ret = (tmp_rand < 90) ? LIGHTGREY : WHITE;
        break;

    case ETC_GOLD:
        ret = (tmp_rand < 60) ? YELLOW : BROWN;
        break;

    case ETC_IRON:
        ret = (tmp_rand < 40) ? CYAN :
              (tmp_rand < 80) ? LIGHTGREY :
                                DARKGREY;
        break;

    case ETC_BONE:
        ret = (tmp_rand < 90) ? WHITE : LIGHTGREY;
        break;

    case ETC_RANDOM:
        ret = 1 + random2(15);              // always random
        break;

    case ETC_FLOOR: // should already be handled
    case ETC_ROCK:  // should already be handled
    default:
        break;
    }

    ASSERT( !is_element_colour( ret ) );

    return ((ret == BLACK) ? GREEN : ret);
}

char index_to_letter(int the_index)
{
    return (the_index + ((the_index < 26) ? 'a' : ('A' - 26)));
}

int letter_to_index(int the_letter)
{
    if (the_letter >= 'a' && the_letter <= 'z')
        // returns range [0-25] {dlb}
        the_letter -= 'a';
    else if (the_letter >= 'A' && the_letter <= 'Z')
        // returns range [26-51] {dlb}
        the_letter -= ('A' - 26);

    return (the_letter);
}

int fuzz_value(int val, int lowfuzz, int highfuzz, int naverage)
{
    const int lfuzz = lowfuzz * val / 100,
        hfuzz = highfuzz * val / 100;
    return val + random2avg(lfuzz + hfuzz + 1, naverage) - lfuzz;
}

// Returns 0 if the point is not near stairs.
// Returns 1 if the point is near unoccupied stairs.
// Returns 2 if the point is near player-occupied stairs.
int near_stairs(const coord_def &p, int max_dist,
                dungeon_char_type &stair_type, branch_type &branch)
{
    for (radius_iterator ri(p, max_dist, true, false); ri; ++ri)
    {
        const dungeon_feature_type feat = grd(*ri);
        if (is_stair(feat))
        {
            // Shouldn't happen for escape hatches.
            if (grid_is_escape_hatch(feat))
                continue;

            stair_type = get_feature_dchar(feat);

            // Is it a branch stair?
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
            return (*ri == you.pos()) ? 2 : 1;
        }
    }

    return (false);
}

bool is_trap_square(dungeon_feature_type grid)
{
    return (grid >= DNGN_TRAP_MECHANICAL && grid <= DNGN_UNDISCOVERED_TRAP);
}

// Does the equivalent of KILL_RESET on all monsters in LOS. Should only be
// applied to new games.
void zap_los_monsters()
{
    losight(env.show, grd, you.pos());

    for (rectangle_iterator ri(crawl_view.vlos1, crawl_view.vlos2); ri; ++ri )
    {
        if (!in_vlos(*ri))
            continue;

        const coord_def g = view2grid(*ri);

        if (!map_bounds(g))
            continue;

        if (g == you.pos())
            continue;

        // At tutorial beginning disallow items in line of sight.
        if (Options.tutorial_events[TUT_SEEN_FIRST_OBJECT])
        {
            int item = igrd(g);

            if (item != NON_ITEM && is_valid_item(mitm[item]) )
                destroy_item(item);
        }

        // If we ever allow starting with a friendly monster,
        // we'll have to check here.
        monsters *mon = monster_at(g);
        if (mon == NULL)
            continue;

        if (mons_class_flag( mon->type, M_NO_EXP_GAIN ))
            continue;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Dismissing %s",
             mon->name(DESC_PLAIN, true).c_str() );
#endif
        // Do a hard reset so the monster's items will be discarded.
        mon->flags |= MF_HARD_RESET;
        // Do a silent, wizard-mode monster_die() just to be extra sure the
        // player sees nothing.
        monster_die(mon, KILL_DISMISSED, NON_MONSTER, true, true);
    }
}

//////////////////////////////////////////////////////////////////////////
// coord_def
int coord_def::distance_from(const coord_def &other) const
{
    return (grid_distance(*this, other));
}

int integer_sqrt(int value)
{
    if (value <= 0)
        return (value);

    int very_old_retval = -1;
    int old_retval = 0;
    int retval = 1;

    while (very_old_retval != old_retval
        && very_old_retval != retval
        && retval != old_retval)
    {
        very_old_retval = old_retval;
        old_retval = retval;
        retval = (value / old_retval + old_retval) / 2;
    }

    return (retval);
}

int random_rod_subtype()
{
    return STAFF_FIRST_ROD + random2(NUM_STAVES - STAFF_FIRST_ROD);
}

int dice_def::roll() const
{
    return roll_dice(this->num, this->size);
}
