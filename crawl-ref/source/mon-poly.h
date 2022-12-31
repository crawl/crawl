/**
 * @file
 * @brief Monster polymorph and mimic functions.
**/

#pragma once

#include "item-def.h"
#include "libutil.h" // always_true

class monster;

bool feature_mimic_at(const coord_def &c);
item_def* item_mimic_at(const coord_def &c);
bool mimic_at(const coord_def &c);

#define ORIGINAL_TYPE_KEY "original_type"
const string POLY_SET_KEY = "poly_set";

enum poly_power_type
{
    PPT_LESS,
    PPT_MORE,
    PPT_SAME,
    PPT_SLIME,
};

void monster_drop_things(
    monster* mons,
    bool mark_item_origins = false,
    bool (*suitable)(const item_def& item) = always_true<const item_def &>);

void change_monster_type(monster* mons, monster_type targetc, bool do_seen=true);
void init_poly_set(monster *mons);
bool monster_polymorph(monster* mons, monster_type targetc,
                       poly_power_type power = PPT_SAME);

void slimify_monster(monster* mons);
bool mon_can_be_slimified(const monster* mons);

void seen_monster(monster* mons);
