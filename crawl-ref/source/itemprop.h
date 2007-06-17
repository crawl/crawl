/*
 *  File:       itemname.cc
 *  Summary:    Misc functions.
 *  Written by: Brent Ross
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        BWR             Created
 */


#ifndef ITEMPROP_H
#define ITEMPROP_H

#include "externs.h"

void init_properties(void);

// cursed:
bool item_cursed( const item_def &item );
bool item_known_cursed( const item_def &item );
bool item_known_uncursed( const item_def &item );
void do_curse_item(  item_def &item );
void do_uncurse_item(  item_def &item );

// ident:
bool item_ident( const item_def &item, unsigned long flags );
void set_ident_flags( item_def &item, unsigned long flags );
void unset_ident_flags( item_def &item, unsigned long flags );
bool fully_identified( const item_def &item );
unsigned long full_ident_mask( const item_def& item );

// racial item and item descriptions:
void set_equip_race( item_def &item, unsigned long flags );
void set_equip_desc( item_def &item, unsigned long flags );
unsigned long get_equip_race( const item_def &item );
unsigned long get_equip_desc( const item_def &item );

// helmet functions:
void  set_helmet_type( item_def &item, short flags );
void  set_helmet_desc( item_def &item, short flags );
void  set_helmet_random_desc( item_def &item );

short get_helmet_type( const item_def &item );
short get_helmet_desc( const item_def &item );

bool  is_helmet_type( const item_def &item, short val );

// ego items:
bool set_item_ego_type( item_def &item, int item_type, int ego_type ); 
int  get_weapon_brand( const item_def &item );
special_armour_type get_armour_ego_type( const item_def &item );
int  get_ammo_brand( const item_def &item );

// armour functions:
int armour_max_enchant( const item_def &item );
bool armour_is_hide( const item_def &item, bool inc_made = false );
bool armour_not_shiny( const item_def &item );
int armour_str_required( const item_def &arm );

equipment_type get_armour_slot( const item_def &item );
equipment_type get_armour_slot( armour_type arm );

bool jewellery_is_amulet( const item_def &item );
bool check_jewellery_size( const item_def &item, size_type size );

bool  hide2armour( item_def &item );

bool  base_armour_is_light( const item_def &item );
int   fit_armour_size( const item_def &item, size_type size );
bool  check_armour_size( const item_def &item, size_type size );
bool  check_armour_shape( const item_def &item, bool quiet );

// weapon functions:
int weapon_rarity( int w_type );

int   cmp_weapon_size( const item_def &item, size_type size );
bool  check_weapon_tool_size( const item_def &item, size_type size );
int   fit_weapon_wieldable_size( const item_def &item, size_type size );
bool  check_weapon_wieldable_size( const item_def &item, size_type size );

int   fit_item_throwable_size( const item_def &item, size_type size );

bool  check_weapon_shape( const item_def &item, bool quiet, bool check_id = false );

int weapon_ev_bonus( const item_def &wpn, int skill, size_type body, int dex,
                     bool hide_hidden = false );

int get_inv_wielded( void );
int get_inv_hand_tool( void );
int get_inv_in_hand( void );

hands_reqd_type  hands_reqd( const item_def &item, size_type size );
hands_reqd_type hands_reqd(object_class_type base_type, int sub_type,
                           size_type size);
bool is_double_ended( const item_def &item );

int double_wpn_awkward_speed( const item_def &item );

bool  is_demonic( const item_def &item );

int   get_vorpal_type( const item_def &item );
int   get_damage_type( const item_def &item );
bool  does_damage_type( const item_def &item, int dam_type );

int weapon_str_weight( const item_def &wpn );
int weapon_dex_weight( const item_def &wpn );
int weapon_impact_mass( const item_def &wpn );
int weapon_str_required( const item_def &wpn, bool half );

skill_type weapon_skill( const item_def &item );
skill_type weapon_skill( object_class_type wclass, int wtype );

skill_type range_skill( const item_def &item );
skill_type range_skill( object_class_type wclass, int wtype );

// launcher and ammo functions:
bool          is_range_weapon( const item_def &item );
bool          is_range_weapon_type( weapon_type wtype );
missile_type  fires_ammo_type( const item_def &item );
missile_type fires_ammo_type( weapon_type wtype );
const char *  ammo_name( const item_def &bow );
bool          is_throwable( const item_def &wpn ); 
launch_retval is_launched( int being_id, const item_def &ammo, bool msg = false );

// staff/rod functions:
bool item_is_rod( const item_def &item );
bool item_is_staff( const item_def &item );

// ring functions:
int ring_has_pluses( const item_def &item );

// food functions:
bool food_is_meat( const item_def &item );
bool food_is_veg( const item_def &item );
int  food_value( const item_def &item );
int  food_turns( const item_def &item );
bool can_cut_meat( const item_def &item );

// generic item property functions:
bool is_tool( const item_def &item );
int property( const item_def &item, int prop_type );
int item_mass( const item_def &item );
size_type item_size( const item_def &item );

bool is_colourful_item( const item_def &item );

bool is_shield(const item_def &item);
bool is_shield_incompatible(const item_def &weapon, 
                            const item_def *shield = NULL);

bool is_deck(const item_def &item);
deck_rarity_type deck_rarity(const item_def &item);

// Only works for armour/weapons/missiles
std::string item_base_name(const item_def &item);
const char* weapon_base_name(unsigned char subtype);

#endif
