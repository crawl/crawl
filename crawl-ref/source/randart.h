/*
 *  File:       randart.h
 *  Summary:    Random and unrandom artefact functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef RANDART_H
#define RANDART_H

#include "externs.h"

// used in files.cc, newgame.cc, randart.cc {dlb}
#define NO_UNRANDARTS 59
#define RA_PROPERTIES RAP_NUM_PROPERTIES

// Reserving the upper bits for later expansion/versioning.
#define RANDART_SEED_MASK  0x00ffffff


bool is_known_artefact( const item_def &item );
bool is_artefact( const item_def &item );
bool is_random_artefact( const item_def &item );
bool is_unrandom_artefact( const item_def &item );
bool is_fixed_artefact( const item_def &item );

unique_item_status_type get_unique_item_status( object_class_type base_type,
                                                int type );
void set_unique_item_status( object_class_type base_type, int type,
                             unique_item_status_type status );

/* ***********************************************************************
 * called from: itemname
 * *********************************************************************** */
std::string get_artefact_name( const item_def &item );

/* ***********************************************************************
 * called from: spl-book
 * *********************************************************************** */
void set_randart_name( item_def &item, const std::string &name );
void set_randart_appearance( item_def &item, const std::string &appear );

/* ***********************************************************************
 * called from: effects
 * *********************************************************************** */
std::string artefact_name( const item_def &item, bool appearance = false );

/* ***********************************************************************
 * called from: describe
 * *********************************************************************** */
const char *unrandart_descrip( int which_descrip, const item_def &item );

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
                              randart_known_props_t &known,
                              bool force_fake_props = false);

void randart_wpn_properties( const item_def &item,
                             randart_properties_t &proprt,
                             randart_known_props_t &known );

void randart_wpn_properties( const item_def &item,
                             randart_properties_t &proprt );

int randart_wpn_property( const item_def &item, randart_prop_type prop,
                          bool &known );

int randart_wpn_property( const item_def &item, randart_prop_type prop );

int randart_known_wpn_property( const item_def &item, randart_prop_type prop );

int randart_wpn_num_props( const item_def &item );
int randart_wpn_num_props( const randart_properties_t &proprt );

void randart_wpn_learn_prop( item_def &item, randart_prop_type prop );
bool randart_wpn_known_prop( const item_def &item, randart_prop_type prop );

/* ***********************************************************************
 * called from: dungeon
 * *********************************************************************** */
bool make_item_fixed_artefact( item_def &item, bool in_abyss, int which = 0 );

bool make_item_blessed_blade( item_def &item );
bool make_item_randart( item_def &item );
bool make_item_unrandart( item_def &item, int unrand_index );

/* ***********************************************************************
 * called from: randart - debug
 * *********************************************************************** */
bool randart_is_bad( const item_def &item );
bool randart_is_bad( const item_def &item, randart_properties_t &proprt );

/* ***********************************************************************
 * called from: files - newgame
 * *********************************************************************** */
void set_unrandart_exist(int whun, bool is_exist);

/* ***********************************************************************
 * called from: items
 * *********************************************************************** */
int find_unrandart_index(const item_def& artefact);
int find_unrandart_index(const item_def &item);

/* ***********************************************************************
 * called from: debug
 * *********************************************************************** */
void randart_set_properties( item_def             &item,
                             randart_properties_t &proprt );
void randart_set_property( item_def          &item,
                           randart_prop_type  prop,
                           int                val );

/* ***********************************************************************
 * called from: mapdef
 * *********************************************************************** */
int get_fixedart_num( const char *name );
#endif
