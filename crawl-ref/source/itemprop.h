/*
 *  File:       itemprop.h
 *  Summary:    Misc functions.
 *  Written by: Brent Ross
 */


#ifndef ITEMPROP_H
#define ITEMPROP_H

#include "itemprop-enum.h"

struct bolt;

void init_properties(void);

// cursed:
bool item_known_cursed( const item_def &item );
bool item_known_uncursed( const item_def &item );
void do_curse_item( item_def &item, bool quiet = true );
void do_uncurse_item(item_def &item, bool inscribe = true);

// stationary:
void set_item_stationary( item_def &item );
void remove_item_stationary( item_def &item );
bool item_is_stationary( const item_def &item );

// ident:
bool item_ident( const item_def &item, iflags_t flags );
void set_ident_flags( item_def &item, iflags_t flags );
void unset_ident_flags( item_def &item, iflags_t flags );
bool fully_identified( const item_def &item );
iflags_t full_ident_mask( const item_def& item );

// racial item and item descriptions:
void set_equip_race( item_def &item, iflags_t flags );
void set_equip_desc( item_def &item, iflags_t flags );
iflags_t get_equip_race( const item_def &item );
iflags_t get_equip_desc( const item_def &item );

// helmet functions:
void  set_helmet_desc( item_def &item, short flags );
void  set_helmet_random_desc( item_def &item );
short get_helmet_desc( const item_def &item );

bool  is_helmet( const item_def& item );
bool  is_hard_helmet( const item_def& item );

short get_gloves_desc( const item_def &item );
void  set_gloves_random_desc( item_def &item );

// ego items:
bool set_item_ego_type( item_def &item, int item_type, int ego_type );
int  get_weapon_brand( const item_def &item );
special_armour_type get_armour_ego_type( const item_def &item );
bool missile_brand_obvious(special_missile_type brand);
special_missile_type get_ammo_brand(const item_def &item);

// armour functions:
int armour_max_enchant( const item_def &item );
bool armour_is_hide( const item_def &item, bool inc_made = false );

equipment_type get_armour_slot( const item_def &item );
equipment_type get_armour_slot( armour_type arm );

bool jewellery_is_amulet( const item_def &item );
bool jewellery_is_amulet( int sub_type );

bool  hide2armour( item_def &item );

bool  base_armour_is_light( const item_def &item );
int   fit_armour_size( const item_def &item, size_type size );
bool  check_armour_size( const item_def &item, size_type size );

bool item_is_rechargeable(const item_def &it, bool hide_charged = false,
                          bool weapons = false);
int wand_charge_value(int type);
bool is_enchantable_weapon(const item_def &wpn, bool uncurse,
                           bool first = true);
bool is_enchantable_armour(const item_def &arm, bool uncurse,
                           bool unknown = false);

bool is_shield(const item_def &item);
bool is_shield_incompatible(const item_def &weapon,
                            const item_def *shield = NULL);
bool shield_reflects(const item_def &shield);
void ident_reflector(item_def *item);

// Only works for armour/weapons/missiles
// weapon functions:
int weapon_rarity( int w_type );

int   cmp_weapon_size( const item_def &item, size_type size );
int   fit_weapon_wieldable_size( const item_def &item, size_type size );
bool  check_weapon_wieldable_size( const item_def &item, size_type size );

int   fit_item_throwable_size( const item_def &item, size_type size );

int weapon_ev_bonus( const item_def &wpn, int skill, size_type body, int dex,
                     bool hide_hidden = false );

hands_reqd_type  hands_reqd( const item_def &item, size_type size );
hands_reqd_type hands_reqd(object_class_type base_type, int sub_type,
                           size_type size);

bool is_demonic(const item_def &item);
bool is_blessed(const item_def &item);
bool is_blessed_convertible(const item_def &item);
bool convert2good(item_def &item, bool allow_blessed = true);
bool convert2bad(item_def &item);

int get_vorpal_type(const item_def &item);
int get_damage_type(const item_def &item);
bool does_damage_type(const item_def &item, int dam_type);
int single_damage_type(const item_def &item);

int weapon_str_weight( const item_def &wpn );
int weapon_dex_weight( const item_def &wpn );

skill_type weapon_skill( const item_def &item );
skill_type weapon_skill( object_class_type wclass, int wtype );

skill_type range_skill( const item_def &item );
skill_type range_skill( object_class_type wclass, int wtype );

// launcher and ammo functions:
bool is_range_weapon(const item_def &item);
bool is_range_weapon_type(weapon_type wtype);
missile_type fires_ammo_type(const item_def &item);
missile_type fires_ammo_type(weapon_type wtype);
const char *ammo_name(missile_type ammo);
const char *ammo_name(const item_def &bow);
bool has_launcher(const item_def &ammo);
bool is_throwable(const actor *actor, const item_def &wpn, bool force = false);
launch_retval is_launched(const actor *actor, const item_def *launcher,
                          const item_def &missile);

// staff/rod functions:
bool item_is_rod( const item_def &item );
bool item_is_staff( const item_def &item );

// Macguffins
bool item_is_rune(const item_def &item, rune_type which_rune = NUM_RUNE_TYPES);
bool item_is_unique_rune(const item_def &item);
bool item_is_orb(const item_def &orb);

bool item_is_corpse(const item_def &item);

// ring functions:
int  ring_has_pluses( const item_def &item );
bool ring_has_stackable_effect( const item_def &item );

// food functions:
bool food_is_meat(const item_def &item);
bool food_is_veg(const item_def &item);
bool is_blood_potion(const item_def &item);
int food_value(const item_def &item);
int food_turns(const item_def &item);
bool can_cut_meat(const item_def &item);
bool food_is_rotten(const item_def &item);
int corpse_freshness(const item_def &item);
bool is_fruit(const item_def & item);
uint32_t item_fruit_mask(const item_def &item);

// generic item property functions:
int property(const item_def &item, int prop_type);
bool gives_ability(const item_def &item);
bool gives_resistance(const item_def &item);
int item_mass(const item_def &item);
size_type item_size(const item_def &item);
equipment_type get_item_slot(object_class_type type, int sub_type);
equipment_type get_item_slot(const item_def& item);

std::string item_base_name(const item_def &item);
std::string item_base_name (object_class_type type, int sub_type);
std::string food_type_name (const item_def &item);
std::string food_type_name (int sub_type);
const char* weapon_base_name(uint8_t subtype);

void seen_item(const item_def &item);
#endif
