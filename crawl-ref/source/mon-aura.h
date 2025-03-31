/**
 * @file
 * @brief Monster passive line-of-sight enchantment functionality.
**/

#pragma once

#include "duration-type.h"
#include "enchant-type.h"

#define PHALANX_BARRIER_KEY "phalanx_barrier"
#define TORPOR_SLOWED_KEY "torpor_slowed"
#define OPHAN_MARK_KEY "ophan_mark"

class mon_enchant;

enum ench_aura_type
{
    AURA_NO,            // Not from an aura effect
    AURA_FRIENDLY,      // From a friendly aura effect
    AURA_HOSTILE,       // From a hostile aura effect
};

bool mons_has_aura(const monster& mon);
bool mons_has_aura_of_type(const monster& mon, enchant_type type);
bool mons_has_aura_of_type(const monster& mon, duration_type type);

bool aura_is_active(const monster& affected, enchant_type type);
bool aura_is_active_on_player(string player_key);

void mons_update_aura(const monster& mon);
