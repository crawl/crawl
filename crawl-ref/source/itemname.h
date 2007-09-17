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

enum item_type_id_type
{
    IDTYPE_WANDS = 0,
    IDTYPE_SCROLLS,
    IDTYPE_JEWELLERY,
    IDTYPE_POTIONS,
    NUM_IDTYPE
};

enum item_type_id_state_type  // used for values in id[4][50]
{
    ID_UNKNOWN_TYPE = 0,
    ID_MON_TRIED_TYPE,
    ID_TRIED_TYPE,
    ID_KNOWN_TYPE
};

// [dshaligram] If you edit potion colours/descriptions, also update 
// itemname.cc.
enum potion_description_colour_type
{
    PDC_CLEAR,
    PDC_BLUE,
    PDC_BLACK,
    PDC_SILVERY,
    PDC_CYAN,
    PDC_PURPLE,
    PDC_ORANGE,
    PDC_INKY,
    PDC_RED,
    PDC_YELLOW,
    PDC_GREEN,
    PDC_BROWN,
    PDC_PINK,
    PDC_WHITE,
    PDC_NCOLOURS
};

// [dshaligram] If you edit potion colours/descriptions, also update 
// itemname.cc.
enum potion_description_qualifier_type
{
    PDQ_NONE,
    PDQ_BUBBLING,
    PDQ_FUMING,
    PDQ_FIZZY,
    PDQ_VISCOUS,
    PDQ_LUMPY,
    PDQ_SMOKY,
    PDQ_GLOWING,
    PDQ_SEDIMENTED,
    PDQ_METALLIC,
    PDQ_MURKY,
    PDQ_GLUGGY,
    PDQ_OILY,
    PDQ_SLIMY,
    PDQ_EMULSIFIED,
    PDQ_NQUALS
};

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
bool item_type_tried( const item_def &item );

bool is_interesting_item( const item_def& item );

std::string make_name( unsigned long seed, bool all_caps );

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void init_properties();

typedef FixedArray < item_type_id_state_type, NUM_IDTYPE, 50 > id_arr;

id_arr& get_typeid_array();
item_type_id_state_type get_ident_type(object_class_type basetype,
                                       int subtype);
void set_ident_type( object_class_type basetype, int subtype,
                     item_type_id_state_type setting, bool force = false);

/* ***********************************************************************
 * called from: command - itemname - invent.h
 * *********************************************************************** */
const std::string menu_colour_item_prefix(const item_def &item);

#endif
