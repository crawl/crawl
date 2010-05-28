/*
 *  File:       delay.h
 *  Summary:    Functions for handling multi-turn actions.
 */

#ifndef DELAY_H
#define DELAY_H

#include "enum.h"

class monsters;
struct ait_hp_loss;

enum activity_interrupt_payload_type
{
    AIP_NONE,
    AIP_INT,
    AIP_STRING,
    AIP_MONSTER,
    AIP_HP_LOSS
};

struct activity_interrupt_data
{
    activity_interrupt_payload_type apt;
    const void *data;
    std::string context;

    activity_interrupt_data()
        : apt(AIP_NONE), data(NULL), context()
    {
    }
    activity_interrupt_data(const int *i)
        : apt(AIP_INT), data(i), context()
    {
    }
    activity_interrupt_data(const char *s)
        : apt(AIP_STRING), data(s), context()
    {
    }
    activity_interrupt_data(const std::string &s)
        : apt(AIP_STRING), data(s.c_str()), context()
    {
    }
    activity_interrupt_data(const monsters *m, const std::string &ctx = "")
        : apt(AIP_MONSTER), data(m), context(ctx)
    {
    }
    activity_interrupt_data(const ait_hp_loss *ahl)
        : apt(AIP_HP_LOSS), data(ahl), context()
    {
    }
    activity_interrupt_data(const activity_interrupt_data &a)
        : apt(a.apt), data(a.data), context(a.context)
    {
    }
};

struct ait_hp_loss
{
    int hp;
    int hurt_type;  // KILLED_BY_POISON, etc.

    ait_hp_loss(int _hp, int _ht) : hp(_hp), hurt_type(_ht) { }
};

void start_delay( delay_type type, int turns, int parm1 = 0, int parm2 = 0 );
void stop_delay( bool stop_stair_travel = false );
bool you_are_delayed();
delay_type current_delay_action();
int check_recital_monster_at(const coord_def& where);
int check_recital_audience();
void handle_delay();
void finish_last_delay();

bool is_run_delay(int delay);
bool is_being_butchered(const item_def &item, bool just_first = true);
bool is_vampire_feeding();
bool is_butchering();
bool player_stair_delay();
bool already_learning_spell(int spell);
void stop_butcher_delay();
void maybe_clear_weapon_swap();
void handle_interrupted_swap(bool swap_if_safe = false,
                             bool force_unsafe = false,
                             bool transform = false);

void clear_macro_process_key_delay();

const char *activity_interrupt_name(activity_interrupt_type ai);
activity_interrupt_type get_activity_interrupt(const std::string &);

const char *delay_name(int delay);
delay_type get_delay(const std::string &);

void autotoggle_autopickup(bool off);
bool interrupt_activity( activity_interrupt_type ai,
                         const activity_interrupt_data &a
                            = activity_interrupt_data() );
#endif
