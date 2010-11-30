/*
 *  File:       mon-behv.h
 *  Summary:    Monster behaviour functions.
 *  Written by: Linley Henzell
 */

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
};

class monster;
struct coord_def;

void behaviour_event(monster* mon, mon_event_type event_type,
                     int src = MHITNOT, coord_def src_pos = coord_def(),
                     bool allow_shout = true);

// This function is somewhat low level; you should probably use
// behaviour_event(mon, ME_EVAL) instead.
void handle_behaviour(monster* mon);

void make_mons_stop_fleeing(monster* mon);

void set_random_target(monster* mon);

void make_mons_leave_level(monster* mon);

bool monster_can_hit_monster(monster* mons, const monster* targ);

// For Zotdef: the target position of MHITYOU monsters is
// the orb, if it's on the ground. Note that zotdef_target()
// returns the orb position, or you.pos if the orb's not
// on the ground.
#define PLAYER_POS (crawl_state.game_is_zotdef() ? zotdef_target() : you.pos())

coord_def zotdef_target();

#endif
