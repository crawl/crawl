/*
 *  File:       delay.h
 *  Summary:    Functions for handling multi-turn actions.
 *  
 *  Change History (most recent first):
 *
 *               <1>    09/09/01        BWR            Created
 */

#ifndef DELAY_H
#define DELAY_H

void start_delay( int type, int turns, int parm1 = 0, int parm2 = 0 );
void stop_delay( void ); 
bool you_are_delayed( void ); 
int  current_delay_action( void ); 
void handle_delay( void );

#endif
