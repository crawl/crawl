/*
 *  File:       shopping.h
 *  Summary:    Shop keeper functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
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

shop_struct *get_shop(const coord_def& where);

std::string shop_name(const coord_def& where);
std::string shop_name(const coord_def& where, bool add_stop);

bool shoptype_identifies_stock(shop_type type);

bool is_shop_item(const item_def &item);
#endif
