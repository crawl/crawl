/**
 * @file
 * @brief Monster behaviour functions.
**/

#ifndef MONBEHV_H
#define MONBEHV_H

#include "enum.h"

enum mon_event_type
{
    ME_EVAL,                            // 0, evaluate monster AI state
    ME_DISTURB,                         // noisy
    ME_ANNOY,                           // annoy at range
    ME_ALERT,                           // alert to presence
    ME_WHACK,                           // physical attack
    ME_SCARE,                           // frighten monster
    ME_CORNERED,                        // cannot flee
    ME_HURT,                            // lost some HP (by any mean)
};

class monster;
struct coord_def;

void behaviour_event(monster* mon, mon_event_type event_type,
                     const actor *agent = 0, coord_def src_pos = coord_def(),
                     bool allow_shout = true);

// This function is somewhat low level; you should probably use
// behaviour_event(mon, ME_EVAL) instead.
void handle_behaviour(monster* mon);

void make_mons_stop_fleeing(monster* mon);

void set_random_target(monster* mon);

void make_mons_leave_level(monster* mon);

bool monster_can_hit_monster(monster* mons, const monster* targ);

bool summon_can_attack(const monster* mons);
bool summon_can_attack(const monster* mons, const coord_def &p);
bool summon_can_attack(const monster* mons, const actor* targ);

void shake_off_monsters(const actor* target);

// For Zotdef: the target position of MHITYOU monsters is
// the orb.
#define PLAYER_POS (crawl_state.game_is_zotdef() ? env.orb_pos : you.pos())

#endif
