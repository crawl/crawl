/*
 *  File:       shopping.cc
 *  Summary:    Shop keeper functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SHOPPING_H
#define SHOPPING_H

#include "externs.h"

int randart_value( const item_def &item );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: chardump - invent - ouch - religion - shopping
 * *********************************************************************** */

// ident == true overrides the item ident level and gives the price
// as if the item was fully id'd
unsigned int item_value( item_def item, char id[4][50], bool ident = false );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: misc
 * *********************************************************************** */
void shop(void);



// last updated 06mar2001 {gdl}
/* ***********************************************************************
 * called from: items direct
 * *********************************************************************** */
const char *shop_name(int sx, int sy);

#endif
