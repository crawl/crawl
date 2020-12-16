#pragma once

#include <vector>

#include "enum.h"
#include "mpr.h"
#include "spl-cast.h"

using std::vector;

struct bolt;
struct dice_def;
class dist;

void setup_fire_storm(const actor *source, int pow, bolt &beam);
spret cast_fire_storm(int pow, bolt &beam, bool fail);
bool cast_smitey_damnation(int pow, bolt &beam);
spret cast_chain_spell(spell_type spell_cast, int pow,
                            const actor *caster, bool fail = false);
string desc_chain_lightning_dam(int pow);

spret trace_los_attack_spell(spell_type spell, int pow,
                                  const actor* agent);
spret fire_los_attack_spell(spell_type spell, int pow, const actor* agent,
                            bool fail = false, int* damage_done = nullptr);
void sonic_damage(bool scream);
bool mons_shatter(monster* caster, bool actual = true);
void shillelagh(actor *wielder, coord_def where, int pow);
spret vampiric_drain(int pow, monster* mons, bool fail);
dice_def vampiric_drain_damage(int pow);
spret cast_freeze(int pow, monster* mons, bool fail);
dice_def freeze_damage(int pow);
spret cast_airstrike(int pow, const dist &beam, bool fail);
spret cast_shatter(int pow, bool fail);
dice_def shatter_damage(int pow, monster *mons = nullptr);
spret cast_irradiate(int powc, actor* who, bool fail);
dice_def irradiate_damage(int powc, bool random = true);
bool ignite_poison_affects(const actor* act);
spret cast_ignite_poison(actor *agent, int pow, bool fail,
                              bool tracer = false);
bool safe_discharge(coord_def where, vector<const actor *> &exclude);
spret cast_discharge(int pow, const actor &agent, bool fail = false,
                          bool prompt = true);
bool setup_fragmentation_beam(bolt &beam, int pow, const actor *caster,
                              const coord_def target, bool quiet,
                              const char **what, bool &hole);
spret cast_fragmentation(int powc, const actor *caster,
                              const coord_def target, bool fail);
pair<int, item_def *> sandblast_find_ammo();
spret cast_sandblast(int powc, bolt &beam, bool fail);
spret cast_tornado(int powc, bool fail);
void tornado_damage(actor *caster, int dur, bool is_vortex = false);
void cancel_tornado(bool tloc = false);
void tornado_move(const coord_def &pos);
coord_def get_thunderbolt_last_aim(actor *caster);
spret cast_thunderbolt(actor *caster, int pow, coord_def aim,
                            bool fail);

actor* forest_near_enemy(const actor *mon);
void forest_message(const coord_def pos, const string &msg,
                    msg_channel_type ch = MSGCH_PLAIN);
void forest_damage(const actor *mon);

spret cast_dazzling_flash(int pow, bool fail, bool tracer = false);

spret cast_toxic_radiance(actor *caster, int pow, bool fail = false,
                               bool mon_tracer = false);
void toxic_radiance_effect(actor* agent, int mult, bool on_cast = false);

spret cast_searing_ray(int pow, bolt &beam, bool fail);
void handle_searing_ray();
void end_searing_ray();

spret cast_glaciate(actor *caster, int pow, coord_def aim,
                         bool fail = false);

spret cast_random_bolt(int pow, bolt& beam, bool fail = false);

vector<coord_def> get_ignition_blast_sources(const actor *agent);
spret cast_ignition(const actor *caster, int pow, bool fail);

spret cast_starburst(int pow, bool fail, bool tracer=false);

void foxfire_attack(const monster *foxfire, const actor *target);

spret cast_hailstorm(int pow, bool fail, bool tracer=false);

spret cast_imb(int pow, bool fail);

void actor_apply_toxic_bog(actor *act);

vector<coord_def> find_ramparts_walls(const coord_def &center);
spret cast_frozen_ramparts(int pow, bool fail);
void end_frozen_ramparts();
dice_def ramparts_damage(int pow, bool random = true);

vector<monster *> find_abszero_possibles(int radius);
spret cast_absolute_zero(int pow, bool fail, bool tracer = false);
