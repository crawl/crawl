/*
 *  File:       invent.cc
 *  Summary:    Functions for inventory related commands.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef INVENT_H
#define INVENT_H

#include <stddef.h>
#include <vector>
#include "menu.h"
#include "enum.h"

#define PROMPT_ABORT        -1
#define PROMPT_GOT_SPECIAL  -2

struct SelItem
{
    int slot;
    int quantity;
    const item_def *item;

    SelItem() : slot(0), quantity(0), item(NULL) { }
    SelItem( int s, int q, const item_def *it = NULL )
        : slot(s), quantity(q), item(it) 
    {
    }
};


int prompt_invent_item( const char *prompt, int type_expect,
                        bool must_exist = true, 
                        bool allow_auto_list = true, 
                        bool allow_easy_quit = true,
                        const char other_valid_char = '\0',
                        int *const count = NULL,
			operation_types oper = OPER_ANY );

std::vector<SelItem> select_items( std::vector<item_def*> &items, 
                                   const char *title, bool noselect = false );

std::vector<SelItem> prompt_invent_items(
                        const char *prompt,
                        int type_expect,
                        std::string (*titlefn)( int menuflags, 
                                                const std::string &oldt ) 
                            = NULL,
                        bool allow_auto_list = true,
                        bool allow_easy_quit = true,
                        const char other_valid_char = '\0',
                        std::vector<text_pattern> *filter = NULL,
                        Menu::selitem_tfn fn = NULL,
                        const std::vector<SelItem> *pre_select = NULL );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: invent - ouch - shopping
 * *********************************************************************** */
unsigned char invent( int item_class_inv, bool show_price );

unsigned char invent_select(int item_class_inv,
                   int select_flags = MF_NOSELECT,
                   std::string (*titlefn)( int menuflags, 
                                           const std::string &oldt ) 
                        = NULL,
                   std::vector<SelItem> *sels = NULL,
                   std::vector<text_pattern> *filter = NULL,
                   Menu::selitem_tfn fn = NULL,
                   const std::vector<SelItem> *pre_select = NULL );

// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr - command - food - item_use - items - spl-book - spells1
 * *********************************************************************** */
unsigned char get_invent(int invent_type);

bool in_inventory(const item_def &i);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void list_commands(bool wizard);

std::string item_class_name(int type, bool terse = false);

void populate_item_menu( Menu *menu, const std::vector<item_def> &items,
                         void (*callback)(MenuEntry *me) );

#endif
