/*
 *  File:       godabil.h
 *  Summary:    God-granted abilities.
 */

#ifndef GODABIL_H
#define GODABIL_H

#include "enum.h"
#include "externs.h"

struct bolt;

bool zin_sustenance(bool actual = true);
bool zin_vitalisation();
void zin_remove_divine_stamina();

bool elyvilon_destroy_weapons();
bool elyvilon_divine_vigour();
void elyvilon_remove_divine_vigour();

bool vehumet_supports_spell(spell_type spell);

bool trog_burn_spellbooks();

bool yred_injury_mirror(bool actual = true);

bool jiyva_accepts_prayer();
void jiyva_paralyse_jellies();
bool jiyva_remove_bad_mutation();

bool beogh_water_walk();

void yred_make_enslaved_soul(monsters *mon, bool force_hostile = false,
                             bool quiet = false, bool unrestricted = false);

bool fedhas_passthrough_class(const monster_type mc);
bool fedhas_passthrough(const monsters * target);
bool fedhas_shoot_through(const bolt & beam, const monsters * victim);
int fedhas_fungal_bloom();
bool fedhas_sunlight();
bool prioritise_adjacent(const coord_def &target,
                         std::vector<coord_def> &candidates);
bool fedhas_plant_ring_from_fruit();
int fedhas_rain(const coord_def &target);
int fedhas_corpse_spores(beh_type behavior = BEH_FRIENDLY,
                         bool interactive = true);
bool mons_is_evolvable(const monsters * mon);
bool fedhas_evolve_flora();

void lugonu_bend_space();

bool is_ponderousifiable(const item_def& item);
bool ponderousify_armour();
int cheibriados_slouch(int pow);
void cheibriados_time_bend(int pow);
void cheibriados_time_step(int pow);
#endif
