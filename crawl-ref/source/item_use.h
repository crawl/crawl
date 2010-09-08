/*
 *  File:       item_use.h
 *  Summary:    Functions for making use of inventory items.
 *  Written by: Linley Henzell
 */


#ifndef ITEM_USE_H
#define ITEM_USE_H


#include <string>
#include "externs.h"
#include "enum.h"

struct bolt;

enum enchant_stat_type
{
    ENCHANT_TO_HIT,
    ENCHANT_TO_DAM,
};

enum fire_type
{
    FIRE_NONE      = 0x0000,
    FIRE_LAUNCHER  = 0x0001,
    FIRE_DART      = 0x0002,
    FIRE_STONE     = 0x0004,
    FIRE_DAGGER    = 0x0008,
    FIRE_JAVELIN   = 0x0010,
    FIRE_SPEAR     = 0x0020,
    FIRE_HAND_AXE  = 0x0040,
    FIRE_CLUB      = 0x0080,
    FIRE_ROCK      = 0x0100,
    FIRE_NET       = 0x0200,
    FIRE_RETURNING = 0x0400,
    FIRE_INSCRIBED = 0x0800,   // Only used for _get_fire_order
};

struct bolt;
class dist;

bool armour_prompt(const std::string & mesg, int *index, operation_types oper);

bool takeoff_armour(int index);

void drink(int slot = -1);

bool elemental_missile_beam(int launcher_brand, int ammo_brand);

bool safe_to_remove_or_wear(const item_def &item, bool remove,
                            bool quiet = false);

void examine_object(void);

bool puton_ring(int slot = -1);

void read_scroll(int slot = -1);

bool remove_ring(int slot = -1, bool announce = false);

bool item_is_quivered(const item_def &item);
int get_next_fire_item(int current, int offset);
int get_ammo_to_shoot(int item, dist &target, bool teleport = false);
void fire_thing(int item = -1);
void throw_item_no_quiver(void);

void wear_armour(int slot = -1);

bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary);

bool do_wear_armour(int item, bool quiet);

struct item_def;

bool can_wield(item_def *weapon, bool say_why = false,
               bool ignore_temporary_disability = false);

bool wield_weapon(bool auto_wield, int slot = -1,
                  bool show_weff_messages = true, bool force = false,
                  bool show_unwield_msg = true,
                  bool show_wield_msg = true);

void zap_wand(int slot = -1);

bool puton_item(int slot);

bool enchant_weapon(enchant_stat_type which_stat, bool quiet, item_def &wpn);
bool enchant_armour(int &ac_change, bool quiet, item_def &arm);

bool setup_missile_beam(const actor *actor, bolt &beam, item_def &item,
                        std::string &ammo_name, bool &returning);

void throw_noise(actor* act, const bolt &pbolt, const item_def &ammo);

bool throw_it(bolt &pbolt, int throw_2, bool teleport = false,
              int acc_bonus = 0, dist *target = NULL);

bool thrown_object_destroyed(item_def *item, const coord_def& where);

void prompt_inscribe_item();
int launcher_shield_slowdown(const item_def &launcher,
                             const item_def *shield);
int launcher_final_speed(const item_def &launcher,
                         const item_def *shield);

void warn_shield_penalties();

bool wearing_slot(int inv_slot);

bool item_blocks_teleport(bool calc_unid, bool permit_id);
bool stasis_blocks_effect(bool calc_unid, bool identify,
                          const char *msg, int noise = 0,
                          const char *silencedmsg = NULL);

#ifdef USE_TILE
void tile_item_use_floor(int idx);
void tile_item_pickup(int idx);
void tile_item_drop(int idx);
void tile_item_eat_floor(int idx);
void tile_item_use(int idx);
void tile_item_use_secondary(int idx);
#endif

#endif
