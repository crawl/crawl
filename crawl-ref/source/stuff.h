/*
 *  File:       stuff.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <3>    11/14/99     cdl    added random40
 *   <2>    11/06/99     cdl    added random22
 *   <1>     -/--/--     LRH    Created
 */


#ifndef STUFF_H
#define STUFF_H

#include "externs.h"

char *const make_time_string(time_t abs_time, char *const buff, int buff_size, bool terse = false);
void set_redraw_status( unsigned long flags );
void tag_followers( void );
void untag_followers( void );

void seed_rng(void);
void seed_rng(long seed);
void push_rng_state();
void pop_rng_state();

void cf_setseed(void);
bool coinflip(void);
int div_rand_round( int num, int den );
bool one_chance_in(int a_million);
int random2(int randmax);
int random_range(int low, int high);
unsigned long random_int(void);
int random2avg( int max, int rolls );
int bestroll(int max, int rolls);

int roll_dice( int num, int size );
int roll_dice( const struct dice_def &dice );
void scale_dice( dice_def &dice, int threshold = 24 );


int random2limit(int max, int limit);
bool see_grid(unsigned char grx, unsigned char gry);
int stepdown_value(int base_value, int stepping, int first_step, int last_step, int ceiling_value);
int stat_mult( int stat_level, int value, int div = 20, int shift = 3 );
int stat_div( int stat_level, int value, int div = 20, int shift = 3 );
int skill_bump( int skill );
unsigned char get_ch(void);

void end(int end_arg);

void modify_all_stats(int STmod, int IQmod, int DXmod);

void redraw_screen(void);

void canned_msg(unsigned char which_message);

bool yesno( const char * str, bool safe = true, int safeanswer = 0, 
            bool clear_after = true );

int yesnoquit( const char* str, bool safe = true,
	       int safeanswer = 0, bool clear_after = true );


bool in_bounds( int x, int y );
bool map_bounds( int x, int y );

int grid_distance( int x, int y, int x2, int y2 );
int distance( int x, int y, int x2, int y2);
bool adjacent( int x, int y, int x2, int y2 );

bool silenced(char x, char y);

bool player_can_hear(char x, char y);

unsigned char random_colour(void);
bool is_element_colour( int col );
int  element_colour( int element, bool no_random = false );

char index_to_letter (int the_index);

int letter_to_index(int the_letter);

int near_stairs(int px, int py, int max_dist, unsigned char &stair_gfx);

inline bool testbits(unsigned int flags, unsigned int test)
{
    return ((flags & test) == test);
}

bool is_trap_square(int x, int y);

void zap_los_monsters();

#endif

