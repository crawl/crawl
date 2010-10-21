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
int zin_check_recite_to_single_monster(const coord_def& where);
int zin_check_recite_to_monsters();
int zin_recite_to_single_monster(const coord_def& where,
                                 bool imprisoned = false, int pow = -1);
bool zin_vitalisation();
void zin_remove_divine_stamina();
bool zin_remove_all_mutations();
bool zin_sanctuary();

void tso_divine_shield();
void tso_remove_divine_shield();

bool elyvilon_destroy_weapons();
void elyvilon_purification();
bool elyvilon_divine_vigour();
void elyvilon_remove_divine_vigour();

bool vehumet_supports_spell(spell_type spell);

bool trog_burn_spellbooks();

bool jiyva_can_paralyse_jellies();
void jiyva_paralyse_jellies();
bool jiyva_remove_bad_mutation();

bool beogh_water_walk();

bool yred_injury_mirror();
bool yred_can_animate_dead();
void yred_animate_remains_or_dead();
void yred_drain_life();
void yred_make_enslaved_soul(monster* mon, bool force_hostile = false);

bool kiku_receive_corpses(int pow, coord_def where);

bool fedhas_passthrough_class(const monster_type mc);
bool fedhas_passthrough(const monster* target);
bool fedhas_passthrough(const monster_info* target);
bool fedhas_shoot_through(const bolt & beam, const monster* victim);
int fedhas_fungal_bloom();
bool fedhas_sunlight();
bool prioritise_adjacent(const coord_def &target,
                         std::vector<coord_def> &candidates);
bool fedhas_plant_ring_from_fruit();
int fedhas_rain(const coord_def &target);
int fedhas_corpse_spores(beh_type behavior = BEH_FRIENDLY,
                         bool interactive = true);
bool mons_is_evolvable(const monster* mon);
bool fedhas_evolve_flora();

void lugonu_bend_space();

bool is_ponderousifiable(const item_def& item);
bool ponderousify_armour();
void cheibriados_time_bend(int pow);
int cheibriados_slouch(int pow);
void cheibriados_time_step(int pow);
bool ashenzari_transfer_knowledge();
#endif
