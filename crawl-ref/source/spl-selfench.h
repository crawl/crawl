
#pragma once

#include "spl-cast.h"
#include "spl-util.h"
#include "transformation.h"


spret cast_deaths_door(int pow, bool fail);
void remove_ice_armour();
spret ice_armour(int pow, bool fail);

int harvest_corpses(const actor &harvester,
                    bool dry_run = false, bool defy_god = false);
spret corpse_armour(int pow, bool fail);

void remove_missile_prot();
spret missile_prot(int pow, bool fail);
spret deflection(int pow, bool fail);

spret cast_regen(int pow, bool fail);
spret cast_revivification(int pow, bool fail);

spret cast_swiftness(int power, bool fail);

int cast_selective_amnesia(const string &pre_msg = "");
spret cast_silence(int pow, bool fail = false);

spret cast_infusion(int pow, bool fail);
spret cast_song_of_slaying(int pow, bool fail);

spret cast_liquefaction(int pow, bool fail);
spret cast_shroud_of_golubria(int pow, bool fail);
spret cast_transform(int pow, transformation which_trans, bool fail);

spret cast_noxious_bog(int pow, bool fail);
void noxious_bog_cell(coord_def p);

spret cast_elemental_weapon(int pow, bool fail);
void end_elemental_weapon(item_def& weapon, bool verbose = false);
void enchant_elemental_weapon(item_def& weapon, spschools_type disciplines, bool verbose = false);

spret cast_flame_strike(int pow, bool fail);

spret cast_insulation(int power, bool fail);

spret change_lesser_lich(int power, bool fail);

spret cast_shrapnel_curtain(int pow, bool fail);