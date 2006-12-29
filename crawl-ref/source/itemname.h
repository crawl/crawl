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
 * called from: describe - effects - item_use - shopping
 * *********************************************************************** */
char get_ident_type(char cla, int ty);


/* ***********************************************************************
 * called from: acr - chardump - direct - effects - fight - invent -
 *              it_use2 - item_use - items - monstuff - mstuff2 - ouch -
 *              shopping - spells1 - spells2 - spells3
 * *********************************************************************** */
const char *item_name( const item_def &item, char descrip, 
                char *buff = NULL,
                bool terse = false );


/* ***********************************************************************
 * called from: debug - describe - dungeon - fight - files - item_use -
 *              monstuff - mstuff2 - players - spells0
 * *********************************************************************** */
int property( const item_def &item, int prop_type );


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void check_item_knowledge(void);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void clear_ids(void);


/* ***********************************************************************
 * called from: direct - fight - food - items - monstuff - religion -
 *              shopping
 * *********************************************************************** */
const char *it_name(int itn, char des, char *buff = NULL, bool terse = false);

/* ***********************************************************************
 * called from: acr - chardump - command - effects - fight - invent -
 *              it_use2 - it_use3 - item_use - items - ouch - output -
 *              spell - spells1 - spells2 - spells3 - spells4 - transfor
 * *********************************************************************** */
const char *in_name(int inn, char des, char *buff = NULL, bool terse = false);

/* ***********************************************************************
 * called from: itemname.cc items.cc item_use.cc mstuff2.cc
 * *********************************************************************** */
const char *quant_name( const item_def &item, int quant, char des, 
                 char buff[ITEMNAME_SIZE], bool terse = false );

/* ***********************************************************************
 * bit operations called from a large number of files
 * *********************************************************************** */
bool item_cursed( const item_def &item );
bool item_known_cursed( const item_def &item );
bool item_known_uncursed( const item_def &item );
bool fully_identified( const item_def &item );
unsigned long full_ident_mask( const item_def& item );

bool item_type_known( const item_def &item );

bool set_item_ego_type( item_def &item, int item_type, int ego_type ); 

int get_weapon_brand( const item_def &item );
int get_ammo_brand( const item_def &item );
int get_armour_ego_type( const item_def &item );

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void init_properties(void);

int make_name( unsigned long seed, bool all_caps, char buff[ ITEMNAME_SIZE ] );

/* ***********************************************************************
 * called from: files - shopping
 * *********************************************************************** */
void save_id(char identy[4][50]);


/* ***********************************************************************
 * called from: files - item_use - newgame - ouch - shopping - spells1
 * *********************************************************************** */
void set_ident_type( char cla, int ty, char setting, bool force = false );


/* ***********************************************************************
 * called from: dungeon - item_use
 * *********************************************************************** */
bool hide2armour( unsigned char *which_subtype );

bool is_interesting_item( const item_def& item );

#endif
