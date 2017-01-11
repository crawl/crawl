#ifndef SPL_DAMAGE_H
#define SPL_DAMAGE_H

#include "enum.h"
#include "spl-cast.h"

struct bolt;
class dist;

spret_type cast_delayed_fireball(bool fail);
void setup_fire_storm(const actor *source, int pow, bolt &beam);
spret_type cast_fire_storm(int pow, bolt &beam, bool fail);
bool cast_smitey_damnation(int pow, bolt &beam);
spret_type cast_chain_spell(spell_type spell_cast, int pow,
                            const actor *caster, bool fail = false);

spret_type trace_los_attack_spell(spell_type spell, int pow, const actor* agent);
spret_type fire_los_attack_spell(spell_type spell, int pow, const actor* agent,
                                 bool fail = false, int* damage_done = nullptr);
void sonic_damage(bool scream);
bool mons_shatter(monster* caster, bool actual = true);
void shillelagh(actor *wielder, coord_def where, int pow);
spret_type vampiric_drain(int pow, monster* mons, bool fail);
spret_type cast_freeze(int pow, monster* mons, bool fail);
spret_type cast_airstrike(int pow, const dist &beam, bool fail);
spret_type cast_shatter(int pow, bool fail);
spret_type cast_irradiate(int powc, actor* who, bool fail);
bool ignite_poison_affects(const actor* act);
spret_type cast_ignite_poison(actor *agent, int pow, bool fail,
                              bool tracer = false);
int discharge_monsters(coord_def where, int pow, actor *agent);
spret_type cast_discharge(int pow, bool fail);
bool setup_fragmentation_beam(bolt &beam, int pow, const actor *caster,
                              const coord_def target, bool allow_random,
                              bool get_max_distance, bool quiet,
                              const char **what,
                              bool &should_destroy_wall, bool &hole);
spret_type cast_fragmentation(int powc, const actor *caster,
                              const coord_def target, bool fail);
spret_type cast_sandblast(int powc, bolt &beam, bool fail);
spret_type cast_tornado(int powc, bool fail);
void tornado_damage(actor *caster, int dur);
void cancel_tornado(bool tloc = false);
void tornado_move(const coord_def &pos);
spret_type cast_thunderbolt(actor *caster, int pow, coord_def aim,
                            bool fail = false);

actor* forest_near_enemy(const actor *mon);
void forest_message(const coord_def pos, const string &msg,
                    msg_channel_type ch = MSGCH_PLAIN);
void forest_damage(const actor *mon);

vector<bolt> get_spray_rays(const actor *caster, coord_def aim, int range,
                            int max_rays, int max_spacing = 3);
spret_type cast_dazzling_spray(int pow, coord_def aim, bool fail = false);

spret_type cast_toxic_radiance(actor *caster, int pow, bool fail = false,
                               bool mon_tracer = false);
void toxic_radiance_effect(actor* agent, int mult);

spret_type cast_searing_ray(int pow, bolt &beam, bool fail);
void handle_searing_ray();
void end_searing_ray();

spret_type cast_glaciate(actor *caster, int pow, coord_def aim,
                         bool fail = false);

spret_type cast_random_bolt(int pow, bolt& beam, bool fail = false);

size_t shotgun_beam_count(int pow);
spret_type cast_scattershot(const actor *caster, int pow, const coord_def &pos,
                            bool fail = false);
#endif
