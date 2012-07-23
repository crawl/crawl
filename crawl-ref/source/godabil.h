/**
 * @file
 * @brief God-granted abilities.
**/

#ifndef GODABIL_H
#define GODABIL_H

#include "enum.h"
#include "externs.h"
#include "mon-info.h"

struct bolt;

std::string zin_recite_text(int* trits, size_t len, int prayertype, int step);
bool zin_check_able_to_recite();
int zin_check_recite_to_monsters(actor *priest, recite_type *prayertype);
bool zin_recite_to_single_monster(actor *priest, const coord_def& where,
                                  recite_type prayertype);
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

bool jiyva_can_paralyse_jellies();
void jiyva_paralyse_jellies();
bool jiyva_remove_bad_mutation();

bool beogh_water_walk();

bool yred_injury_mirror();
bool yred_can_animate_dead();
void yred_animate_remains_or_dead();
void yred_drain_life();
void yred_make_enslaved_soul(monster* mon, bool force_hostile = false);

bool kiku_receive_corpses(int pow, coord_def where, actor *agent);
bool kiku_take_corpse();

bool fedhas_passthrough_class(const monster_type mc);
bool fedhas_passthrough(const monster* target);
bool fedhas_passthrough(const monster_info* target);
bool fedhas_shoot_through(const bolt & beam, const monster* victim);
int fedhas_fungal_bloom(actor* agent, bool check_only = false);
bool fedhas_sunlight(actor* agent);
void process_sunlights(bool future = false);
bool prioritise_adjacent(actor* agent,
                         const coord_def &target,
                         std::vector<coord_def> &candidates);
bool fedhas_plant_ring_from_fruit(actor* agent, bool check_only = false);
int fedhas_rain(actor* agent, const coord_def &target);
int fedhas_corpse_spores(actor* agent,
                         bool hostile = false,
                         bool check_only = false);
bool mons_is_evolvable(const monster* mon);
bool fedhas_evolve_flora(actor* agent, bool check_only = false);

void lugonu_bend_space();

void cheibriados_time_bend(int pow);
void cheibriados_temporal_distortion();
int slouchable(coord_def where, int pow, int, actor* agent);
int slouch_monsters(coord_def where, int pow, int dummy, actor* agent);
bool cheibriados_slouch(int pow);
void cheibriados_time_step(int pow);
bool ashenzari_transfer_knowledge();
bool ashenzari_end_transfer(bool finished = false, bool force = false);
#endif
