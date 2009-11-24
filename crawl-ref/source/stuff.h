/*
 *  File:       stuff.h
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 */


#ifndef STUFF_H
#define STUFF_H

#include "env.h"
#include "player.h"
#include <map>

std::string make_time_string(time_t abs_time, bool terse = false);
std::string make_file_time(time_t when);

void set_redraw_status(unsigned long flags);
void tag_followers();
void untag_followers();

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

int stepdown_value(int base_value, int stepping, int first_step,
                   int last_step, int ceiling_value);
int stat_mult( int stat_level, int value, int div = 20, int shift = 3 );
int stat_div( int stat_level, int value, int div = 20, int shift = 3 );
int skill_bump( int skill );
unsigned char get_ch();

void cio_init();
void cio_cleanup();
void clear_globals_on_exit();
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

bool player_can_hear(const coord_def& p);

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
void zap_los_monsters(bool items_also);

int integer_sqrt(int value);

int random_rod_subtype();

maybe_bool frombool(bool b);
bool tobool(maybe_bool mb, bool def);
bool tobool(maybe_bool mb);

#endif
