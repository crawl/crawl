/*
 *  File:       shopping.cc
 *  Summary:    Shop keeper functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-07-29 18:10:16 +0100 (Sun, 29 Jul 2007) $
 *
 *  Modified for Hexcrawl by Martin Bays, 2007
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SHOPPING_H
#define SHOPPING_H

#include "externs.h"
#include "itemname.h"

int randart_value( const item_def &item );

// ident == true overrides the item ident level and gives the price
// as if the item was fully id'd
unsigned int item_value( item_def item, bool ident = false );
void shop();

shop_struct *get_shop(int sx, int sy);

std::string shop_name(int sx, int sy);
std::string shop_name(int sx, int sy, bool add_stop);

bool shoptype_identifies_stock(shop_type type);

#endif
