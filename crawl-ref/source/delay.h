/**
 * @file
 * @brief Functions for handling multi-turn actions.
**/

#ifndef DELAY_H
#define DELAY_H

#include "enum.h"

class monster;
struct ait_hp_loss;

enum activity_interrupt_payload_type
{
    AIP_NONE,
    AIP_INT,
    AIP_STRING,
    AIP_MONSTER,
    AIP_HP_LOSS,
};

struct activity_interrupt_data
{
    activity_interrupt_payload_type apt;
    union
    {
        const char* string_data;
        const int*  int_data;
        const ait_hp_loss* ait_hp_loss_data;
        monster* mons_data;
        nullptr_t no_data;
    };

    seen_context_type context;

    activity_interrupt_data()
        : apt(AIP_NONE), no_data(nullptr), context(SC_NONE)
    {
    }
    activity_interrupt_data(const int *i)
        : apt(AIP_INT), int_data(i), context(SC_NONE)
    {
    }
    activity_interrupt_data(const char *s)
        : apt(AIP_STRING), string_data(s), context(SC_NONE)
    {
    }
    activity_interrupt_data(const string &s)
        : apt(AIP_STRING), string_data(s.c_str()), context(SC_NONE)
    {
    }
    activity_interrupt_data(monster* m, seen_context_type ctx = SC_NONE)
        : apt(AIP_MONSTER), mons_data(m), context(ctx)
    {
    }
    activity_interrupt_data(const ait_hp_loss *ahl)
        : apt(AIP_HP_LOSS), ait_hp_loss_data(ahl), context(SC_NONE)
    {
    }
};

struct ait_hp_loss
{
    int hp;
    int hurt_type;  // KILLED_BY_POISON, etc.

    ait_hp_loss(int _hp, int _ht) : hp(_hp), hurt_type(_ht) { }
};

void start_delay(delay_type type, int turns, int parm1 = 0, int parm2 = 0,
                 int parm3 = 0);
void stop_delay(bool stop_stair_travel = false, bool force_unsafe = false);
bool you_are_delayed();
delay_type current_delay_action();
void handle_delay();

bool delay_is_run(delay_type delay);
bool is_being_drained(const item_def &item);
bool is_being_butchered(const item_def &item, bool just_first = true);
bool is_vampire_feeding();
bool is_butchering();
bool player_stair_delay();
bool already_learning_spell(int spell = -1);

void clear_macro_process_key_delay();

activity_interrupt_type get_activity_interrupt(const string &);
bool is_delay_interruptible(delay_type delay);

const char *delay_name(int delay);
delay_type get_delay(const string &);

void run_macro(const char *macroname = nullptr);

void autotoggle_autopickup(bool off);
bool interrupt_activity(activity_interrupt_type ai,
                        const activity_interrupt_data &a
                            = activity_interrupt_data(),
                        vector<string>* msgs_buf = nullptr);
#endif
