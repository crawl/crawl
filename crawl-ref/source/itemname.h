/*
 *  File:       itemname.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef ITEMNAME_H
#define ITEMNAME_H

#include "externs.h"

bool is_vowel( const char chr );

/* ***********************************************************************
 * called from: debug - describe - dungeon - fight - files - item_use -
 *              monstuff - mstuff2 - players - spells0
 * *********************************************************************** */
int property( const item_def &item, int prop_type );


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void check_item_knowledge();

std::string quant_name( const item_def &item, int quant,
                        description_level_type des, bool terse = false );

bool item_type_known( const item_def &item );

bool is_interesting_item( const item_def& item );

int make_name( unsigned long seed, bool all_caps, char buff[ ITEMNAME_SIZE ] );

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void init_properties();

id_arr& get_typeid_array();
item_type_id_state_type get_ident_type(object_class_type basetype,
                                       int subtype);
void set_ident_type( object_class_type basetype, int subtype,
                     item_type_id_state_type setting, bool force = false);

#endif
