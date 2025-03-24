
#pragma once

#include "spl-cast.h"
#include "transformation.h"

// Maximum amount of slaying you can stack with fugue
const int FUGUE_MAX_STACKS = 7;

class actor;

spret cast_deaths_door(int pow, bool fail);
void remove_ice_armour();
spret ice_armour(int pow, bool fail);

void fiery_armour();

int harvest_corpses(const actor &harvester,
                    bool dry_run = false, bool defy_god = false);
spret corpse_armour(int pow, bool fail);

spret cast_revivification(int pow, bool fail);

spret cast_swiftness(int power, bool fail);

int cast_selective_amnesia(const string &pre_msg = "");

int silence_min_range(int pow);
int silence_max_range(int pow);
spret cast_silence(int pow, bool fail = false);

spret cast_fugue_of_the_fallen(int pow, bool fail);
void do_fugue_wail(const coord_def pos);

int liquefaction_max_range(int pow);
spret cast_liquefaction(int pow, bool fail);

bool jinxbite_targets_available();
spret cast_jinxbite(int pow, bool fail);

spret cast_confusing_touch(int power, bool fail);

spret cast_detonation_catalyst(bool fail);
