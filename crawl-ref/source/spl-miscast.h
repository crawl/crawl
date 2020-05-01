/**
 * @file
 * @brief Spell miscast class.
**/

#pragma once

#include "actor.h"
#include "spl-util.h"

// scale miscast severity
const int MISCAST_DIVISOR = 9;

// cutoff for getting a miscast
const int MISCAST_THRESHOLD = 150;

enum class miscast_source
{
    hell_effect,
    melee,
    spell,
    wizard,
    deck,
    god
};

class actor;
// class monster;

struct miscast_source_info
{
    // The source of the miscast
    miscast_source source;
    // If source == miscast_source::god, the god the miscast was created by
    god_type god;
};


void miscast_effect(spell_type spell, int fail);
void miscast_effect(actor& target, actor* source, miscast_source_info mc_info,
                    spschool school, int level, int fail, string cause);
