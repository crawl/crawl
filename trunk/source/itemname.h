/*
 *  File:       itemname.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
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
char get_ident_type(char cla, char ty);


/* ***********************************************************************
 * called from: acr - chardump - direct - effects - fight - invent -
 *              it_use2 - item_use - items - monstuff - mstuff2 - ouch -
 *              shopping - spells1 - spells2 - spells3
 * *********************************************************************** */
char item_name( const item_def &item, char descrip, char buff[ITEMNAME_SIZE],
                bool terse = false );


/* ***********************************************************************
 * called from: beam - describe - fight - item_use - items - monstuff -
 *              player
 * *********************************************************************** */
int mass_item( const item_def &item );


/* ***********************************************************************
 * called from: debug - describe - dungeon - fight - files - item_use -
 *              monstuff - mstuff2 - players - spells0
 * *********************************************************************** */
int property( const item_def &item, int prop_type );


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
unsigned char check_item_knowledge(void);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void clear_ids(void);


/* ***********************************************************************
 * called from: direct - fight - food - items - monstuff - religion -
 *              shopping
 * *********************************************************************** */
void it_name(int itn, char des, char buff[ITEMNAME_SIZE], bool terse = false);

/* ***********************************************************************
 * called from: acr - chardump - command - effects - fight - invent -
 *              it_use2 - it_use3 - item_use - items - ouch - output -
 *              spell - spells1 - spells2 - spells3 - spells4 - transfor
 * *********************************************************************** */
void in_name(int inn, char des, char buff[ITEMNAME_SIZE], bool terse = false);

/* ***********************************************************************
 * called from: itemname.cc items.cc item_use.cc mstuff2.cc
 * *********************************************************************** */
void quant_name( const item_def &item, int quant, char des, 
                 char buff[ITEMNAME_SIZE], bool terse = false );

/* ***********************************************************************
 * bit operations called from a large number of files
 * *********************************************************************** */
bool item_cursed( const item_def &item );
bool item_uncursed( const item_def &item );

bool item_known_cursed( const item_def &item );
bool item_known_uncursed( const item_def &item );
// bool fully_indentified( const item_def &item );

bool item_ident( const item_def &item, unsigned long flags );
bool item_not_ident( const item_def &item, unsigned long flags );

void do_curse_item(  item_def &item );
void do_uncurse_item(  item_def &item );

void set_ident_flags( item_def &item, unsigned long flags );
void unset_ident_flags( item_def &item, unsigned long flags );

void set_equip_race( item_def &item, unsigned long flags );
void set_equip_desc( item_def &item, unsigned long flags );

unsigned long get_equip_race( const item_def &item );
unsigned long get_equip_desc( const item_def &item );

bool cmp_equip_race( const item_def &item, unsigned long val );
bool cmp_equip_desc( const item_def &item, unsigned long val );

void set_helmet_type( item_def &item, short flags );
void set_helmet_desc( item_def &item, short flags );
void set_helmet_random_desc( item_def &item );

short get_helmet_type( const item_def &item );
short get_helmet_desc( const item_def &item );

bool cmp_helmet_type( const item_def &item, short val );
bool cmp_helmet_desc( const item_def &item, short val );

bool set_item_ego_type( item_def &item, int item_type, int ego_type ); 

int get_weapon_brand( const item_def &item );
int get_ammo_brand( const item_def &item );
int get_armour_ego_type( const item_def &item );

bool item_is_rod( const item_def &item );
bool item_is_staff( const item_def &item );

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void init_properties(void);

/* ***********************************************************************
 * called from: files - randart - shopping
 * *********************************************************************** */
void make_name( unsigned char var1, unsigned char var2, unsigned char var3,
                char ncase, char buff[ITEMNAME_SIZE] );


/* ***********************************************************************
 * called from: files - shopping
 * *********************************************************************** */
void save_id(char identy[4][50]);


/* ***********************************************************************
 * called from: files - item_use - newgame - ouch - shopping - spells1
 * *********************************************************************** */
void set_ident_type( char cla, char ty, char setting, bool force = false );


/* ***********************************************************************
 * called from: dungeon - item_use
 * *********************************************************************** */
bool hide2armour( unsigned char *which_subtype );


#endif
