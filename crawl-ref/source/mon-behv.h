/**
 * @file
 * @brief Monster behaviour functions.
**/

#pragma once

#include <vector>

#include "beh-type.h"
#include "coord-def.h"
#include "enum.h"
#include "mon-attitude-type.h"

#define MON_SPELL_USABLE_KEY "mon_spell_usable"

#define BULLET_TIME_METHOD_KEY "bullet_time_method"

class actor;

enum mon_event_type
{
    ME_EVAL,                            // 0, evaluate monster AI state
    ME_DISTURB,                         // noisy
    ME_ANNOY,                           // annoy at range
    ME_ALERT,                           // alert to presence
    ME_WHACK,                           // physical attack
    ME_SCARE,                           // frighten monster
    ME_CORNERED,                        // cannot flee
};

enum class bullet_time_method
{
    pending = -1,
    none = 0,
    armour,
    evasion,
    shield,
};

class monster;
struct coord_def;

void behaviour_event(monster* mon, mon_event_type event_type,
                     const actor *agent = 0, coord_def src_pos = coord_def(),
                     bool allow_shout = true);

// This function is somewhat low level; you should probably use
// behaviour_event(mon, ME_EVAL) instead.
void handle_behaviour(monster* mon);

beh_type attitude_creation_behavior(mon_attitude_type att);

void alert_nearby_monsters();

void make_mons_stop_fleeing(monster* mon);

void make_mons_leave_level(monster* mon);

bool monster_needs_los(const monster* mons);
bool monster_los_is_valid(const monster* mons, const coord_def &p);
bool monster_los_is_valid(const monster* mons, const actor* targ);

void shake_off_monsters(const actor* target);

void set_nearest_monster_foe(monster* mon, bool near_player = false);

vector<monster *> find_allies_targeting(const actor &a);
bool is_ally_target(const actor &a);

void mons_end_withdraw_order(monster& mons);

bullet_time_method get_bullet_time(const actor& defender);
void set_bullet_time(actor& defender, bullet_time_method method = bullet_time_method::pending);
bool is_bullet_time(const actor& defender, bullet_time_method method = bullet_time_method::none);
