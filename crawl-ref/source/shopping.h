/*
 *  File:       shopping.cc
 *  Summary:    Shop keeper functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SHOPPING_H
#define SHOPPING_H

#include "externs.h"
#include "itemname.h"

void shop_init_id_type(int shoptype, id_fix_arr &shop_id);
void shop_uninit_id_type(int shoptype, const id_fix_arr &shop_id);

int randart_value( const item_def &item );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: chardump - invent - ouch - religion - shopping
 * *********************************************************************** */

// ident == true overrides the item ident level and gives the price
// as if the item was fully id'd
unsigned int item_value( item_def item, id_arr id, bool ident = false );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: misc
 * *********************************************************************** */
void shop(void);

const shop_struct *get_shop(int sx, int sy);

// last updated 06mar2001 {gdl}
/* ***********************************************************************
 * called from: items direct
 * *********************************************************************** */
const char *shop_name(int sx, int sy);

// Protect the id array against being clobbered by a SIGHUP with the
// character in a shop.
extern id_arr shop_backup_id;
class shopping_hup_protect
{
public:
    shopping_hup_protect() : shopping(crawl_state.shopping)
    {
        save_id(shop_backup_id);
        crawl_state.shopping = true;
    }

    ~shopping_hup_protect()
    {
        crawl_state.shopping = shopping;
    }

private:
    bool shopping;
};

#endif
