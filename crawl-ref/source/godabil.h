/**
 * @file
 * @brief God-granted abilities.
**/

#ifndef GODABIL_H
#define GODABIL_H

#include "enum.h"
#include "externs.h"

struct bolt;

string zin_recite_text(const int seed, const int prayertype, int step);
bool zin_check_able_to_recite(bool quiet = false);
int zin_check_recite_to_monsters(recite_type *prayertype);
bool zin_recite_to_single_monster(const coord_def& where,
                                  recite_type prayertype);
void zin_recite_interrupt();
bool zin_vitalisation();
void zin_remove_divine_stamina();
bool zin_remove_all_mutations();
bool zin_sanctuary();

void tso_divine_shield();
void tso_remove_divine_shield();

void elyvilon_purification();
bool elyvilon_divine_vigour();
void elyvilon_remove_divine_vigour();

bool vehumet_supports_spell(spell_type spell);

bool trog_burn_spellbooks();

void jiyva_paralyse_jellies();
bool jiyva_remove_bad_mutation();

bool beogh_water_walk();

bool yred_injury_mirror();
bool yred_can_animate_dead();
void yred_animate_remains_or_dead();
void yred_make_enslaved_soul(monster* mon, bool force_hostile = false);

bool kiku_receive_corpses(int pow, coord_def where);
bool kiku_take_corpse();

bool fedhas_passthrough_class(const monster_type mc);
bool fedhas_passthrough(const monster* target);
bool fedhas_passthrough(const monster_info* target);
bool fedhas_shoot_through(const bolt& beam, const monster* victim);
int fedhas_fungal_bloom();
bool fedhas_sunlight();
void process_sunlights(bool future = false);
bool prioritise_adjacent(const coord_def& target, vector<coord_def>& candidates);
bool fedhas_plant_ring_from_fruit();
int fedhas_rain(const coord_def &target);
int fedhas_corpse_spores(beh_type attitude = BEH_FRIENDLY,
                         bool interactive = true);
bool mons_is_evolvable(const monster* mon);
bool fedhas_evolve_flora();

void lugonu_bend_space();

void cheibriados_time_bend(int pow);
void cheibriados_temporal_distortion();
bool cheibriados_slouch(int pow);
void cheibriados_time_step(int pow);
bool ashenzari_transfer_knowledge();
bool ashenzari_end_transfer(bool finished = false, bool force = false);

bool can_convert_to_beogh();
void spare_beogh_convert();
#endif
