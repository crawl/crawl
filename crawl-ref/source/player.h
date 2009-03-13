/*
 *  File:       player.cc
 *  Summary:    Player related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef PLAYER_H
#define PLAYER_H

#include "externs.h"
#include "itemprop.h"

class monsters;
struct item_def;

enum genus_type
{
    GENPC_DRACONIAN,                   //    0
    GENPC_ELVEN,                       //    1
    GENPC_DWARVEN,                     //    2
    GENPC_OGRE
};

bool move_player_to_grid( const coord_def& p, bool stepped, bool allow_shift,
                          bool force, bool swapping = false );

bool player_in_mappable_area(void);
bool player_in_branch( int branch );
bool player_in_hell( void );

int get_player_wielded_weapon(void);
bool berserk_check_wielded_weapon(void);
int player_equip( equipment_type slot, int sub_type, bool calc_unid = true );
int player_equip_ego_type( int slot, int sub_type );
int player_damage_type( void );
int player_damage_brand( void );
bool player_can_hit_monster(const monsters *mons);

bool player_is_shapechanged(void);

/* ***********************************************************************
 * called from: player - item_use
 * *********************************************************************** */
bool is_light_armour( const item_def &item );

/* ***********************************************************************
 * called from: beam - fight - misc - newgame
 * *********************************************************************** */
bool player_light_armour(bool with_skill = false);


/* ***********************************************************************
 * called from: acr.cc - fight.cc - misc.cc - player.cc
 * *********************************************************************** */
bool player_in_water(void);
bool player_is_swimming(void);
bool player_is_airborne(void);

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

bool extrinsic_amulet_effect(jewellery_type amulet);
bool wearing_amulet(jewellery_type which_am, bool calc_unid = true);


/* ***********************************************************************
 * called from: acr - chardump - describe - newgame - view
 * *********************************************************************** */
std::string species_name( species_type speci, int level, bool genus = false,
                          bool adj = false );
int str_to_species(const std::string &species);

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
int carrying_capacity(burden_state_type bs = BS_OVERLOADED);


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
int player_energy(void);


/* ***********************************************************************
 * called from: beam - chardump - fight - files - misc - output
 * *********************************************************************** */
int player_evasion(void);


/* ***********************************************************************
 * called from: acr - spells1
 * *********************************************************************** */
int player_movement_speed(void);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int player_hunger_rate(void);

int calc_hunger(int food_cost);

/* ***********************************************************************
 * called from: debug - it_use3 - spells0
 * *********************************************************************** */
int player_mag_abil(bool is_weighted);
int player_magical_power( void );

/* ***********************************************************************
 * called from: fight - misc - ouch - spells
 * *********************************************************************** */
int player_prot_life(bool calc_unid = true, bool temp = true,
                     bool items = true);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int player_regen(void);


/* ***********************************************************************
 * called from: fight - files - it_use2 - misc - ouch - spells - spells2
 * *********************************************************************** */
int player_res_cold(bool calc_unid = true, bool temp = true,
                    bool items = true);
int player_res_acid(bool calc_unid = true, bool items = true);
int player_acid_resist_factor();

int player_res_torment(bool calc_unid = true);

bool player_res_corrosion(bool calc_unid = true);
bool player_item_conserve(bool calc_unid = true);
int player_mental_clarity(bool calc_unid = true, bool items = true);

bool player_can_smell();
bool player_likes_chunks(bool permanently = false);
bool player_can_swim();
bool player_likes_water(bool permanently = false);

bool player_is_unholy();
int player_mutation_level(mutation_type mut);

/* ***********************************************************************
 * called from: fight - files - ouch
 * *********************************************************************** */
int player_res_electricity(bool calc_unid = true, bool temp = true,
                           bool items = true);


/* ***********************************************************************
 * called from: acr - fight - misc - ouch - spells
 * *********************************************************************** */
int player_res_fire(bool calc_unid = true, bool temp = true,
                    bool items = true);
int player_res_sticky_flame(bool calc_unid = true, bool temp = true,
                            bool items = true);
int player_res_steam(bool calc_unid = true, bool temp = true,
                     bool items = true);


/* ***********************************************************************
 * called from: beam - decks - fight - fod - it_use2 - misc - ouch -
 *              spells - spells2
 * *********************************************************************** */
int player_res_poison(bool calc_unid = true, bool temp = true,
                      bool items = true);

bool player_control_teleport(bool calc_unid = true, bool temp = true,
                             bool items = true);

int player_res_magic(void);

bool player_res_asphyx();

/* ***********************************************************************
 * called from: beam - chardump - fight - misc - output
 * *********************************************************************** */
int player_shield_class(void);


int player_spec_air(void);
int player_spec_cold(void);
int player_spec_conj(void);
int player_spec_death(void);
int player_spec_earth(void);
int player_spec_ench(void);
int player_spec_fire(void);
int player_spec_holy(void);
int player_spec_poison(void);
int player_spec_summ(void);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int player_speed(void);


/* ***********************************************************************
 * called from: chardump - spells
 * *********************************************************************** */
int player_spell_levels(void);

/* ***********************************************************************
 * called from: libgui - item_use
 * *********************************************************************** */
bool player_knows_spell(int spell);

// last updated 18may2000 {dlb}
/* ***********************************************************************
 * called from: effects
 * *********************************************************************** */
int player_sust_abil(bool calc_unid = true);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int player_teleport(bool calc_unid = true);


/* ***********************************************************************
 * called from: ability - acr - items - misc - spells1 - spells3
 * *********************************************************************** */
bool items_give_ability(const int slot, randart_prop_type abil);
int scan_randarts(randart_prop_type which_property, bool calc_unid = true);


/* ***********************************************************************
 * called from: fight - item_use
 * *********************************************************************** */
int slaying_bonus(char which_affected);


/* ***********************************************************************
 * called from: beam - decks - direct - effects - fight - files - it_use2 -
 *              items - monstuff - mon-util - mstuff2 - spells1 - spells2 -
 *              spells3
 * *********************************************************************** */
int player_see_invis(bool calc_unid = true);
bool player_monster_visible(const monsters *mon);

bool player_mesmerised_by(const monsters *mon);
void update_beholders(const monsters *mon, bool force = false);
void check_beholders();

/* ***********************************************************************
 * called from: acr - decks - it_use2 - ouch
 * *********************************************************************** */
unsigned long exp_needed(int lev);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int get_expiration_threshold(duration_type dur);
bool dur_expiring(duration_type dur);
void display_char_status(void);


/* ***********************************************************************
 * called from: item_use - items - misc - spells - spells3
 * *********************************************************************** */
void forget_map(unsigned char chance_forgotten = 100, bool force = false);


// last updated 19may2000 {dlb}
/* ***********************************************************************
 * called from: acr - fight
 * *********************************************************************** */
void gain_exp(unsigned int exp_gained, unsigned int* actual_gain = NULL,
              unsigned int* actual_avail_gain = NULL);

// last updated 17dec2000 {gdl}
/* ***********************************************************************
 * called from: acr - it_use2 - item_use - mutation - transfor - player -
 *              misc - stuff
 * *********************************************************************** */
void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const std::string& cause, bool see_source = true);
void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const char* cause, bool see_source = true);
void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const monsters* cause);
void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const item_def &cause, bool removed = false);


// last updated 19may2000 {dlb}
/* ***********************************************************************
 * called from: decks - it_use2 - player
 * *********************************************************************** */
void level_change(bool skip_attribute_increase = false);


/* ***********************************************************************
 * called from: ability - fight - item_use - mutation - newgame - spells0 -
 *              transfor
 * *********************************************************************** */
bool player_genus( genus_type which_genus,
                   species_type species = SP_UNKNOWN );
bool is_player_same_species( const int mon, bool = false );

bool you_can_wear( int eq, bool special_armour = false );
bool player_has_feet(void);
bool player_wearing_slot( int eq );
bool you_tran_can_wear(const item_def &item);
bool you_tran_can_wear( int eq, bool check_mutation = false );

bool enough_hp(int minimum, bool suppress_msg);
bool enough_mp(int minimum, bool suppress_msg, bool include_items = true);

void dec_hp(int hp_loss, bool fatal, const char *aux = NULL);
void dec_mp(int mp_loss);

void inc_mp(int mp_gain, bool max_too);
void inc_hp(int hp_gain, bool max_too);

void rot_hp( int hp_loss );
void unrot_hp( int hp_recovered );
int player_rotted();
void rot_mp( int mp_loss );

void inc_max_hp( int hp_gain );
void dec_max_hp( int hp_loss );

void inc_max_mp( int mp_gain );
void dec_max_mp( int mp_loss );

void deflate_hp(int new_level, bool floor);
void set_hp(int new_amount, bool max_too);

int get_real_hp(bool trans, bool rotted = false);
int get_real_mp(bool include_items);

void set_mp(int new_amount, bool max_too);

void contaminate_player(int change, bool controlled = false,
                        bool status_only = false);

bool confuse_player(int amount, bool resistable = true);

bool curare_hits_player(int death_source, int amount);
bool poison_player(int amount, bool force = false);
void dec_poison_player();
void reduce_poison_player(int amount);

bool napalm_player(int amount);
void dec_napalm_player();

bool slow_player(int amount);
void dec_slow_player();

void haste_player(int amount);
void dec_haste_player();

bool disease_player(int amount);
void dec_disease_player();

bool rot_player(int amount);

bool player_has_spell(spell_type spell);
size_type player_size(int psize = PSIZE_TORSO, bool base = false);

bool player_weapon_wielded();
item_def *player_weapon();
item_def *player_shield();

// Determines if the given grid is dangerous for the player to enter.
bool is_grid_dangerous(int grid);

void run_macro(const char *macroname = NULL);

int player_ghost_base_movement_speed();

int count_worn_ego( int which_ego );
int stat_modifier( stat_type stat );

#endif
