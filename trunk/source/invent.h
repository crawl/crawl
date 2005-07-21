/*
 *  File:       invent.cc
 *  Summary:    Functions for inventory related commands.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef INVENT_H
#define INVENT_H

#include <stddef.h>

#define PROMPT_ABORT        -1
#define PROMPT_GOT_SPECIAL  -2

int prompt_invent_item( const char *prompt, int type_expect,
                        bool must_exist = true, 
                        bool allow_auto_list = true, 
                        bool allow_easy_quit = true,
                        const char other_valid_char = '\0',
                        int *const count = NULL );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: invent - ouch - shopping
 * *********************************************************************** */
unsigned char invent(int item_class_inv, bool show_price);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr - command - food - item_use - items - spl-book - spells1
 * *********************************************************************** */
unsigned char get_invent(int invent_type);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void list_commands(bool wizard);


#endif
