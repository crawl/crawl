/*
 *  File:       delay.h
 *  Summary:    Functions for handling multi-turn actions.
 *  
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>    09/09/01        BWR            Created
 */

#ifndef DELAY_H
#define DELAY_H

#include "enum.h"

struct monsters;
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
void stop_delay( void );
bool you_are_delayed( void ); 
delay_type current_delay_action( void ); 
void handle_delay( void );

bool is_run_delay(int delay);
bool is_being_butchered(const item_def &item);
void stop_butcher_delay();

const char *activity_interrupt_name(activity_interrupt_type ai);
activity_interrupt_type get_activity_interrupt(const std::string &);

const char *delay_name(int delay);
delay_type get_delay(const std::string &);

void perform_activity();
bool interrupt_activity( activity_interrupt_type ai, 
                         const activity_interrupt_data &a 
                            = activity_interrupt_data() );

#endif
