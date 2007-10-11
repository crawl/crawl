/*
 *  File:       randart.cc
 *  Summary:    Random and unrandom artefact functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef RANDART_H
#define RANDART_H

#include "enum.h"
#include "externs.h"

enum randart_prop_type
{
    RAP_BRAND,                         //    0
    RAP_AC,
    RAP_EVASION,
    RAP_STRENGTH,
    RAP_INTELLIGENCE,
    RAP_DEXTERITY,                     //    5
    RAP_FIRE,
    RAP_COLD,
    RAP_ELECTRICITY,
    RAP_POISON,
    RAP_NEGATIVE_ENERGY,               //   10
    RAP_MAGIC,
    RAP_EYESIGHT,
    RAP_INVISIBLE,
    RAP_LEVITATE,
    RAP_BLINK,                         //   15
    RAP_CAN_TELEPORT,
    RAP_BERSERK,
    RAP_MAPPING,
    RAP_NOISES,
    RAP_PREVENT_SPELLCASTING,          //   20
    RAP_CAUSE_TELEPORTATION,
    RAP_PREVENT_TELEPORTATION,
    RAP_ANGRY,
    RAP_METABOLISM,
    RAP_MUTAGENIC,                     //   25
    RAP_ACCURACY,
    RAP_DAMAGE,
    RAP_CURSED,
    RAP_STEALTH,
    RAP_MAGICAL_POWER,                 //   30
    RAP_NUM_PROPERTIES
};

// used in files.cc, newgame.cc, randart.cc {dlb}
#define NO_UNRANDARTS 53
#define RA_PROPERTIES RAP_NUM_PROPERTIES

// Reserving the upper bits for later expansion/versioning.
#define RANDART_SEED_MASK  0x00ffffff


bool is_artefact( const item_def &item );
bool is_random_artefact( const item_def &item );
bool is_unrandom_artefact( const item_def &item );
bool is_fixed_artefact( const item_def &item );

unique_item_status_type get_unique_item_status( int base_type, int type );
void set_unique_item_status( int base_type, int type,
                             unique_item_status_type status );

/* ***********************************************************************
 * called from: itemname
 * *********************************************************************** */
std::string randart_armour_name( const item_def &item );

/* ***********************************************************************
 * called from: itemname
 * *********************************************************************** */
std::string randart_name( const item_def &item );

/* ***********************************************************************
 * called from: itemname
 * *********************************************************************** */
std::string randart_jewellery_name( const item_def &item );

/* ***********************************************************************
 * called from: describe
 * *********************************************************************** */
const char *unrandart_descrip( char which_descrip, const item_def &item );

bool does_unrandart_exist(int whun);

/* ***********************************************************************
 * called from: dungeon
 * *********************************************************************** */
int find_okay_unrandart(unsigned char aclass, unsigned char atype = OBJ_RANDOM);

typedef FixedVector< int, RA_PROPERTIES >  randart_properties_t;
typedef FixedVector< bool, RA_PROPERTIES > randart_known_props_t;

/* ***********************************************************************
 * called from: describe - fight - it_use2 - item_use - player
 * *********************************************************************** */
void randart_desc_properties( const item_def &item, 
                              randart_properties_t &proprt,
                              randart_known_props_t &known );

void randart_wpn_properties( const item_def &item, 
                             randart_properties_t &proprt,
                             randart_known_props_t &known );

void randart_wpn_properties( const item_def &item, 
                             randart_properties_t &proprt );

int randart_wpn_property( const item_def &item, int prop,
                          bool &known );

int randart_wpn_property( const item_def &item, int prop );

int randart_wpn_num_props( const item_def &item );
int randart_wpn_num_props( const randart_properties_t &proprt );

void randart_wpn_learn_prop( item_def &item, int prop );
bool randart_wpn_known_prop( item_def &item, int prop );

/* ***********************************************************************
 * called from: dungeon
 * *********************************************************************** */
bool make_item_fixed_artefact( item_def &item, bool in_abyss, int which = 0 );

bool make_item_randart( item_def &item );
bool make_item_unrandart( item_def &item, int unrand_index );

/* ***********************************************************************
 * called from: files - newgame
 * *********************************************************************** */
void set_unrandart_exist(int whun, bool is_exist);

/* ***********************************************************************
 * called from: items
 * *********************************************************************** */
int find_unrandart_index(int item_index);

#endif
