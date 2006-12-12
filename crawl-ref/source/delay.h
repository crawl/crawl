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

void start_delay( int type, int turns, int parm1 = 0, int parm2 = 0 );
void stop_delay( void ); 
bool you_are_delayed( void ); 
int  current_delay_action( void ); 
void handle_delay( void );

bool is_run_delay(int delay);
const char *activity_interrupt_name(activity_interrupt_type ai);
activity_interrupt_type get_activity_interrupt(const std::string &);

const char *delay_name(int delay);
delay_type get_delay(const std::string &);

void perform_activity();
bool interrupt_activity( activity_interrupt_type ai, 
                         const activity_interrupt_data &a 
                            = activity_interrupt_data() );

#endif
