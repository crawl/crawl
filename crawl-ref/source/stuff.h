/*
 *  File:       stuff.h
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef STUFF_H
#define STUFF_H

#include "externs.h"
#include <map>

std::string make_time_string(time_t abs_time, bool terse = false);
void set_redraw_status(unsigned long flags);
void tag_followers();
void untag_followers();

void seed_rng();
void seed_rng(long seed);
void push_rng_state();
void pop_rng_state();

void cf_setseed();
bool coinflip();
int div_rand_round( int num, int den );
int div_round_up( int num, int den );
bool one_chance_in(int a_million);
bool x_chance_in_y(int x, int y);
int random2(int randmax);
int maybe_random2( int x, bool random_factor );
int random_range(int low, int high);
int random_range(int low, int high, int nrolls);
const char* random_choose_string(const char* first, ...);
int random_choose(int first, ...);
int random_choose_weighted(int weight, int first, ...);
unsigned long random_int();
int random2avg( int max, int rolls );
int bestroll(int max, int rolls);

int roll_dice( int num, int size );
void scale_dice( dice_def &dice, int threshold = 24 );

// Various ways to iterate over things.

// stack_iterator guarantees validity so long as you don't manually
// mess with item_def.link: i.e., you can kill the item you're
// examining but you can't kill the item linked to it.
class stack_iterator : public std::iterator<std::forward_iterator_tag,
                                            item_def>
{
public:
    explicit stack_iterator( const coord_def& pos );
    explicit stack_iterator( int start_link );

    operator bool() const;
    item_def& operator *() const;
    item_def* operator->() const;
    int link() const;

    const stack_iterator& operator ++ ();
    stack_iterator operator ++ (int);
private:
    int cur_link;
    int next_link;
};

class rectangle_iterator :
    public std::iterator<std::forward_iterator_tag, coord_def>
{
public:
    rectangle_iterator( const coord_def& corner1, const coord_def& corner2 );
    explicit rectangle_iterator( int x_border_dist, int y_border_dist = -1 );
    operator bool() const;
    coord_def operator *() const;
    const coord_def* operator->() const;

    rectangle_iterator& operator ++ ();
    rectangle_iterator operator ++ (int);
private:
    coord_def current, topleft, bottomright;
};

class radius_iterator : public std::iterator<std::bidirectional_iterator_tag,
                        coord_def>
{
public:
    radius_iterator( const coord_def& center, int radius,
                     bool roguelike_metric = true,
                     bool require_los = true,
                     bool exclude_center = false );
    bool done() const;
    void reset();
    operator bool() const { return !done(); }
    coord_def operator *() const;
    const coord_def* operator->() const;

    const radius_iterator& operator ++ ();
    const radius_iterator& operator -- ();
    radius_iterator operator ++ (int);
    radius_iterator operator -- (int);
private:
    void step();
    void step_back();
    bool on_valid_square() const;

    coord_def location, center;
    int radius;
    bool roguelike_metric, require_los, exclude_center;
    bool iter_done;
};

class adjacent_iterator : public radius_iterator
{
public:
    explicit adjacent_iterator( const coord_def& pos = you.pos(),
                                bool _exclude_center = true ) :
        radius_iterator(pos, 1, true, false, _exclude_center) {}
};

int random2limit(int max, int limit);
int stepdown_value(int base_value, int stepping, int first_step,
                   int last_step, int ceiling_value);
int stat_mult( int stat_level, int value, int div = 20, int shift = 3 );
int stat_div( int stat_level, int value, int div = 20, int shift = 3 );
int skill_bump( int skill );
unsigned char get_ch();

int fuzz_value(int val, int lowfuzz, int highfuzz, int naverage = 2);

void cio_init();
void cio_cleanup();
void end(int exit_code, bool print_err = false,
         const char *message = NULL, ...);

void modify_all_stats(int STmod, int IQmod, int DXmod);

void redraw_screen();

void canned_msg(canned_message_type which_message);

bool yes_or_no( const char* fmt, ... );
typedef std::map<int, int> explicit_keymap;
bool yesno( const char * str, bool safe = true, int safeanswer = 0,
            bool clear_after = true, bool interrupt_delays = true,
            bool noprompt = false,
            const explicit_keymap *map = NULL );

int yesnoquit( const char* str, bool safe = true, int safeanswer = 0,
               bool allow_all = false, bool clear_after = true,
               char alt_yes = 'Y', char alt_yes2 = 'Y' );

bool in_bounds_x(int x);
bool in_bounds_y(int y);
bool in_bounds(int x, int y);
bool map_bounds_x(int x);
bool map_bounds_y(int y);
bool map_bounds(int x, int y);
coord_def random_in_bounds();

inline bool in_bounds(const coord_def &p)
{
    return in_bounds(p.x, p.y);
}

inline bool map_bounds(const coord_def &p)
{
    return map_bounds(p.x, p.y);
}

// Determines if the coordinate is within bounds of an LOS array.
inline bool show_bounds(const coord_def &p)
{
    return (p.x >= 0 && p.x < ENV_SHOW_DIAMETER
            && p.y >= 0 && p.y < ENV_SHOW_DIAMETER);
}

int grid_distance( const coord_def& p1, const coord_def& p2 );
int grid_distance( int x, int y, int x2, int y2 );
int distance( const coord_def& p1, const coord_def& p2 );
int distance( int x, int y, int x2, int y2);
bool adjacent( const coord_def& p1, const coord_def& p2 );

bool silenced(const coord_def& p);

bool player_can_hear(const coord_def& p);

unsigned char random_colour();
unsigned char random_uncommon_colour();
bool is_element_colour( int col );
int  element_colour( int element, bool no_random = false );

char index_to_letter (int the_index);

int letter_to_index(int the_letter);

int near_stairs(const coord_def &p, int max_dist,
                dungeon_char_type &stair_type, branch_type &branch);

inline bool testbits(unsigned long flags, unsigned long test)
{
    return ((flags & test) == test);
}

template <typename Z> inline Z sgn(Z x)
{
    return (x < 0 ? -1 : (x > 0 ? 1 : 0));
}

bool is_trap_square(dungeon_feature_type grid);
void zap_los_monsters();

class rng_save_excursion
{
public:
    rng_save_excursion(long seed) { push_rng_state(); seed_rng(seed); }
    rng_save_excursion()          { push_rng_state(); }
    ~rng_save_excursion()         { pop_rng_state(); }
};

template<typename Iterator>
int choose_random_weighted(Iterator beg, const Iterator end)
{
    ASSERT(beg < end);

#if DEBUG
    int times_set = 0;
#endif

    int totalweight = 0;
    int count = 0, result = 0;
    while ( beg != end )
    {
        totalweight += *beg;
        if ( random2(totalweight) < *beg )
        {
            result = count;
#if DEBUG
            times_set++;
#endif
        }
        ++count;
        ++beg;
    }
    ASSERT(times_set > 0);
    return result;
}

int integer_sqrt(int value);

int random_rod_subtype();

#endif
