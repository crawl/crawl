/*
 *  File:       player.cc
 *  Summary:    Player related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef PLAYER_H
#define PLAYER_H

#include "externs.h"

bool move_player_to_grid( int x, int y, bool stepped, bool allow_shift,
                          bool force );

bool player_in_mappable_area(void);
bool player_in_branch( int branch );
bool player_in_hell( void );

int get_player_wielded_weapon();
int player_equip( int slot, int sub_type, bool calc_unid = true );
int player_equip_ego_type( int slot, int sub_type );
int player_damage_type( void );
int player_damage_brand( void );

bool player_is_shapechanged(void);

/* ***********************************************************************
 * called from: player - item_use
 * *********************************************************************** */
bool is_light_armour( const item_def &item );

/* ***********************************************************************
 * called from: beam - fight - misc - newgame
 * *********************************************************************** */
bool player_light_armour(void);


/* ***********************************************************************
 * called from: acr.cc - fight.cc - misc.cc - player.cc
 * *********************************************************************** */
bool player_in_water(void);
bool player_is_swimming(void);
bool player_is_levitating(void);

/* ***********************************************************************
 * called from: ability - chardump - fight - religion - spell - spells -
 *              spells0 - spells2
 * *********************************************************************** */
bool player_under_penance(void);

int player_wielded_item();

/* ***********************************************************************
 * called from: ability - acr - fight - food - it_use2 - item_use - items -
 *              misc - mutation - ouch
 * *********************************************************************** */
bool wearing_amulet(char which_am, bool calc_unid = true);


/* ***********************************************************************
 * called from: acr - chardump - describe - newgame - view
 * *********************************************************************** */
char *species_name( int speci, int level, bool genus = false, bool adj = false, bool cap = true );


/* ***********************************************************************
 * called from: beam
 * *********************************************************************** */
bool you_resist_magic(int power);


/* ***********************************************************************
 * called from: acr - decks - effects - it_use2 - it_use3 - item_use -
 *              items - output - shopping - spells1 - spells3
 * *********************************************************************** */
int burden_change(void);


/* ***********************************************************************
 * called from: items - misc
 * *********************************************************************** */
int carrying_capacity(void);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int check_stealth(void);


/* ***********************************************************************
 * called from: bang - beam - chardump - fight - files - it_use2 -
 *              item_use - misc - output - spells - spells1
 * *********************************************************************** */
int player_AC(void);


/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
unsigned char player_energy(void);


/* ***********************************************************************
 * called from: beam - chardump - fight - files - misc - output
 * *********************************************************************** */
int player_evasion(void);


#if 0
/* ***********************************************************************
 * called from: acr - spells1
 * *********************************************************************** */
unsigned char player_fast_run(void);
#endif

/* ***********************************************************************
 * called from: acr - spells1
 * *********************************************************************** */
int player_movement_speed(void);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int player_hunger_rate(void);


/* ***********************************************************************
 * called from: debug - it_use3 - spells0
 * *********************************************************************** */
int player_mag_abil(bool is_weighted);
int player_magical_power( void );

/* ***********************************************************************
 * called from: fight - misc - ouch - spells
 * *********************************************************************** */
int player_prot_life(bool calc_unid = true);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int player_regen(void);


/* ***********************************************************************
 * called from: fight - files - it_use2 - misc - ouch - spells - spells2
 * *********************************************************************** */
int player_res_cold(bool calc_unid = true);
int player_res_acid(void);

bool player_res_corrosion(bool calc_unid = true);
bool player_item_conserve(bool calc_unid = true);
int player_mental_clarity(bool calc_unid = true);


/* ***********************************************************************
 * called from: fight - files - ouch
 * *********************************************************************** */
int player_res_electricity(bool calc_unid = true);


/* ***********************************************************************
 * called from: acr - fight - misc - ouch - spells
 * *********************************************************************** */
int player_res_fire(bool calc_unid = true);


/* ***********************************************************************
 * called from: beam - decks - fight - fod - it_use2 - misc - ouch -
 *              spells - spells2
 * *********************************************************************** */
int player_res_poison(bool calc_unid = true);

bool player_control_teleport(bool calc_unid = true);

int player_res_magic(void);

bool player_res_asphyx();

/* ***********************************************************************
 * called from: beam - chardump - fight - misc - output
 * *********************************************************************** */
int player_shield_class(void);


/* ***********************************************************************
 * called from: spell - spells0
 * *********************************************************************** */
unsigned char player_spec_air(void);


/* ***********************************************************************
 * called from: spell - spells0
 * *********************************************************************** */
unsigned char player_spec_cold(void);


/* ***********************************************************************
 * called from: spell - spells0
 * *********************************************************************** */
unsigned char player_spec_conj(void);


/* ***********************************************************************
 * called from: it_use3 - spell - spells0
 * *********************************************************************** */
unsigned char player_spec_death(void);


/* ***********************************************************************
 * called from: spell - spells0
 * *********************************************************************** */
unsigned char player_spec_earth(void);


/* ***********************************************************************
 * called from: spell - spells0
 * *********************************************************************** */
unsigned char player_spec_ench(void);


/* ***********************************************************************
 * called from: spell - spells0
 * *********************************************************************** */
unsigned char player_spec_fire(void);


/* ***********************************************************************
 * called from: spell - spells0
 * *********************************************************************** */
unsigned char player_spec_holy(void);


/* ***********************************************************************
 * called from: spell - spells0
 * *********************************************************************** */
unsigned char player_spec_poison(void);


/* ***********************************************************************
 * called from: spell - spells0
 * *********************************************************************** */
unsigned char player_spec_summ(void);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int player_speed(void);


/* ***********************************************************************
 * called from: chardump - spells
 * *********************************************************************** */
int player_spell_levels(void);


// last updated 18may2000 {dlb}
/* ***********************************************************************
 * called from: effects
 * *********************************************************************** */
unsigned char player_sust_abil(bool calc_unid = true);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int player_teleport(bool calc_unid = true);


/* ***********************************************************************
 * called from: ability - acr - items - misc - spells1 - spells3
 * *********************************************************************** */
int scan_randarts(char which_property, bool calc_unid = true);


/* ***********************************************************************
 * called from: fight - item_use
 * *********************************************************************** */
int slaying_bonus(char which_affected);


/* ***********************************************************************
 * called from: beam - decks - direct - effects - fight - files - it_use2 -
 *              items - monstuff - mon-util - mstuff2 - spells1 - spells2 -
 *              spells3
 * *********************************************************************** */
unsigned char player_see_invis(bool calc_unid = true);
bool player_monster_visible( const monsters *mon );


/* ***********************************************************************
 * called from: acr - decks - it_use2 - ouch
 * *********************************************************************** */
unsigned long exp_needed(int lev);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void display_char_status(void);


/* ***********************************************************************
 * called from: item_use - items - misc - spells - spells3
 * *********************************************************************** */
void forget_map(unsigned char chance_forgotten);


// last updated 19may2000 {dlb}
/* ***********************************************************************
 * called from: acr - fight
 * *********************************************************************** */
void gain_exp(unsigned int exp_gained);


// last updated 17dec2000 {gdl}
/* ***********************************************************************
 * called from: acr - it_use2 - item_use - mutation - transfor - player -
 *              misc - stuff
 * *********************************************************************** */
void modify_stat(unsigned char which_stat, char amount, bool suppress_msg);


// last updated 19may2000 {dlb}
/* ***********************************************************************
 * called from: decks - it_use2 - player
 * *********************************************************************** */
void level_change(void);


/* ***********************************************************************
 * called from: skills
 * *********************************************************************** */
void redraw_skill(const char your_name[kNameLen], const char class_name[80]);


/* ***********************************************************************
 * called from: ability - fight - item_use - mutation - newgame - spells0 -
 *              transfor
 * *********************************************************************** */
bool player_genus( unsigned char which_genus,
                   unsigned char species = SP_UNKNOWN );


/* ***********************************************************************
 * called from: ability - effects - fight - it_use3 - ouch - spell -
 *              spells - spells2 - spells3 - spells4
 * *********************************************************************** */
void dec_hp(int hp_loss, bool fatal);


/* ***********************************************************************
 * called from: ability - it_use3 - spell - spells3
 * *********************************************************************** */
bool enough_hp (int minimum, bool suppress_msg);


/* ***********************************************************************
 * called from: ability - it_use3
 * *********************************************************************** */
bool enough_mp (int minimum, bool suppress_msg);


/* ***********************************************************************
 * called from: ability - fight - it_use3 - monstuff - ouch - spell
 * *********************************************************************** */
void dec_mp(int mp_loss);


/* ***********************************************************************
 * called from: ability - acr - fight - it_use2 - it_use3 - spells3
 * *********************************************************************** */
void inc_mp(int mp_gain, bool max_too);


/* ***********************************************************************
 * called from: acr - fight - food - spells1 - spells2
 * *********************************************************************** */
void inc_hp(int hp_gain, bool max_too);

void rot_hp( int hp_loss );
void unrot_hp( int hp_recovered );
int player_rotted( void );
void rot_mp( int mp_loss );

void inc_max_hp( int hp_gain );
void dec_max_hp( int hp_loss );

void inc_max_mp( int mp_gain );
void dec_max_mp( int mp_loss );

/* ***********************************************************************
 * called from: acr - misc - religion - skills2 - spells1 - transfor
 * *********************************************************************** */
void deflate_hp(int new_level, bool floor);


/* ***********************************************************************
 * called from: acr - it_use2 - newgame - ouch - religion - spell - spells1
 * *********************************************************************** */
void set_hp(int new_amount, bool max_too);


/* ***********************************************************************
 * called from: it_use3 - newgame
 * *********************************************************************** */
void set_mp(int new_amount, bool max_too);


/* ***********************************************************************
 * called from: newgame
 * *********************************************************************** */
int get_species_index_by_abbrev( const char *abbrev );
int get_species_index_by_name( const char *name );
const char *get_species_abbrev( int which_species );

int get_class_index_by_abbrev( const char *abbrev );
int get_class_index_by_name( const char *name );
const char *get_class_abbrev( int which_job );
const char *get_class_name( int which_job );


// last updated 19apr2001 {gdl}
/* ***********************************************************************
 * called from:
 * *********************************************************************** */
void contaminate_player(int change, bool statusOnly = false);

void poison_player( int amount, bool force = false );
void reduce_poison_player( int amount );

void confuse_player( int amount, bool resistable = true );
void reduce_confuse_player( int amount );

void slow_player( int amount );
void dec_slow_player();

void haste_player( int amount );
void dec_haste_player();

void disease_player( int amount );
void dec_disease_player();

void rot_player( int amount );

void perform_activity();

void interrupt_activity( activity_interrupt_type ai, 
                     const activity_interrupt_data &a 
                            = activity_interrupt_data() );

// last updated 15sep2001 {bwr}
/* ***********************************************************************
 * called from:
 * *********************************************************************** */
bool player_has_spell( int spell );

bool player_weapon_wielded();

void run_macro(const char *macroname = NULL);

#endif
