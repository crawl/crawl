#pragma once

#include <vector>

#include "enum.h"
#include "mpr.h"
#include "spl-cast.h"

using std::vector;

struct bolt;
struct dice_def;
class dist;

const int DEFAULT_SHATTER_DICE = 3;
const int FLAT_DISCHARGE_ARC_DAMAGE = 3;
const int AIRSTRIKE_PER_SPACE_BONUS = 2;
const int MAX_AIRSTRIKE_BONUS = 8 * AIRSTRIKE_PER_SPACE_BONUS;
const int GRAVE_CLAW_MAX_CHARGES = 3;

#define COUPLING_TIME_KEY "maxwells_charge_time"
#define FLAME_WAVE_POWER_KEY "flame_wave_power"
#define FROZEN_RAMPARTS_POWER_KEY "frozen_ramparts_power"
#define TOXIC_RADIANCE_POWER_KEY "toxic_radiance_power"
#define VORTEX_POWER_KEY "vortex_power"
#define FUSILLADE_POWER_KEY "fusillade_power"
#define GRAVE_CLAW_CHARGES_KEY "grave_claw_charges"
#define FORTRESS_BLAST_POS_KEY "fortress_blast_pos"


void setup_fire_storm(const actor *source, int pow, bolt &beam);
spret cast_fire_storm(int pow, bolt &beam, bool fail);
bool cast_smitey_damnation(int pow, bolt &beam);
spret cast_chain_spell(spell_type spell_cast, int pow,
                            const actor *caster, bool fail = false);
spret cast_chain_lightning(int pow, const actor &caster, bool fail);
vector<coord_def> chain_lightning_targets();

spret trace_los_attack_spell(spell_type spell, int pow,
                                  const actor* agent);
spret fire_los_attack_spell(spell_type spell, int pow, const actor* agent,
                            bool fail = false, int* damage_done = nullptr);
int adjacent_huddlers(coord_def pos, bool only_in_sight = false);
void sonic_damage(bool scream);
bool mons_shatter(monster* caster, bool actual = true);
void shillelagh(actor *wielder, coord_def where, int pow);
spret cast_freeze(int pow, monster* mons, bool fail);
spret cast_airstrike(int pow, coord_def target, bool fail);
int airstrike_space_around(coord_def target, bool count_invis);
dice_def base_airstrike_damage(int pow, bool random = false);
string describe_airstrike_dam(dice_def dice);
string airstrike_intensity_display(int empty_space, tileidx_t& tile);
string describe_resonance_strike_dam(dice_def dice);
spret cast_momentum_strike(int pow, coord_def target, bool fail);
spret cast_shatter(int pow, bool fail);
dice_def shatter_damage(int pow, monster *mons = nullptr, bool random = false);
int terrain_shatter_chance(coord_def where, const actor &agent);
spret cast_irradiate(int powc, actor &caster, bool fail);
dice_def irradiate_damage(int powc, bool random = true);
bool ignite_poison_affects_cell(const coord_def where, actor* agent);
spret cast_ignite_poison(actor *agent, int pow, bool fail,
                              bool tracer = false);
spret cast_unravelling(coord_def target, int pow, bool fail);
string mons_inner_flame_immune_reason(const monster *mons);
spret cast_inner_flame(coord_def target, int pow, bool fail);
int get_mercury_weaken_chance(int victim_hd, int pow);
dice_def poisonous_vapours_damage(int pow, bool random);
spret cast_poisonous_vapours(const actor& agent, int pow, const coord_def target, bool fail);
bool safe_discharge(coord_def where, bool check_only = false,
                    bool exclude_center = true);
void discharge_at_location(int pow, const actor &agent, coord_def location);
spret cast_discharge(int pow, const actor &agent, bool fail = false,
                          bool prompt = true);
int discharge_max_damage(int pow);
spret cast_arcjolt(int pow, const actor &agent, bool fail);
dice_def arcjolt_damage(int pow, bool random);
vector<coord_def> arcjolt_targets(const actor &agent, bool actual);
vector<coord_def> galvanic_targets(const actor &agent, coord_def pos, bool actual);
void do_galvanic_jolt(const actor& agent, coord_def pos, dice_def damage);
bool mons_should_fire_plasma(int pow, const actor &agent);
spret cast_plasma_beam(int pow, const actor &agent, bool fail);
vector<coord_def> plasma_beam_targets(const actor &agent, int pow, bool actual);
vector<coord_def> plasma_beam_paths(coord_def source, const vector<coord_def> &targets);
dice_def base_fragmentation_damage(int pow, bool random);
bool setup_fragmentation_beam(bolt &beam, int pow, const actor *caster,
                              const coord_def target, bool quiet,
                              const char **what, bool &hole);
spret cast_fragmentation(int powc, const actor *caster,
                              const coord_def target, bool fail);
spret cast_polar_vortex(int powc, bool fail, bool no_prompt = false);
void polar_vortex_damage(actor *caster, int dur);
dice_def polar_vortex_dice(int pow, bool random);
void cancel_polar_vortex(bool tloc = false);
coord_def get_thunderbolt_last_aim(actor *caster);
dice_def thunderbolt_damage(int power, int arc);
spret cast_thunderbolt(actor *caster, int pow, coord_def aim,
                            bool fail);
bool mons_should_fire_permafrost(int pow, const actor &agent);
spret cast_permafrost_eruption(actor &caster, int pow, bool fail);
set<coord_def> permafrost_targets(const actor &caster, int pow, bool actual = true);

actor* forest_near_enemy(const actor *mon);
void forest_message(const coord_def pos, const string &msg,
                    msg_channel_type ch = MSGCH_PLAIN);
void forest_damage(const actor *mon);

int dazzle_chance_numerator(int hd);
int dazzle_chance_denom(int pow);
bool dazzle_target(actor *victim, const actor *agent, int pow);
spret cast_dazzling_flash(const actor *caster, int pow, bool fail, bool tracer = false);

spret cast_toxic_radiance(actor *caster, int pow, bool fail = false,
                               bool tracer = false);
void toxic_radiance_effect(actor* agent, int mult, bool on_cast = false);

dice_def glaciate_damage(int pow, int eff_range);
spret cast_glaciate(actor *caster, int pow, coord_def aim,
                         bool fail = false);

spret cast_scorch(const actor& agent, int pow, bool fail = false);
dice_def scorch_damage(int pow, bool random);

vector<coord_def> get_ignition_blast_sources(const actor *agent,
                                             bool tracer = false);
spret cast_ignition(const actor *caster, int pow, bool fail);

spret cast_starburst(int pow, bool fail, bool tracer=false);

void foxfire_attack(const monster *foxfire, const actor *target);

spret cast_hailstorm(int pow, bool fail, bool tracer=false);

spret cast_flame_wave(int pow, bool fail);
void handle_flame_wave(int lvl);

spret cast_imb(int pow, bool fail);

dice_def toxic_bog_damage();
void actor_apply_toxic_bog(actor *act);

vector<coord_def> find_ramparts_walls();
spret cast_frozen_ramparts(int pow, bool fail);
void end_frozen_ramparts();
dice_def ramparts_damage(int pow, bool random = true);

spret cast_searing_ray(actor& agent, int pow, bolt &beam, bool fail);
bool handle_searing_ray(actor& agent, int turn);

vector<monster *> find_maxwells_possibles();
spret cast_maxwells_coupling(int pow, bool fail, bool tracer = false);
void handle_maxwells_coupling();
void end_maxwells_coupling(bool quiet = false);

spret cast_noxious_bog(int pow, bool fail);
vector<coord_def> find_bog_locations(const coord_def &center, int pow);

vector<coord_def> find_near_hostiles(int range, bool affect_invis,
                                     const actor& agent);

int siphon_essence_range();
bool siphon_essence_affects(const monster &m);

void attempt_jinxbite_hit(actor& victim);
dice_def boulder_damage(int pow, bool random);
void do_boulder_impact(monster& boulder, actor& victim, bool quiet = false);

dice_def electrolunge_damage(int pow);

int get_warp_space_chance(int pow);

dice_def default_collision_damage(int pow, bool random);
string describe_collision_dam(dice_def dice);

vector<coord_def> get_magnavolt_targets();
vector<coord_def> get_magnavolt_beam_paths(vector<coord_def>& targets);
spret cast_magnavolt(coord_def target, int pow, bool fail);

spret cast_fulsome_fusillade(int pow, bool fail);
void fire_fusillade();

spret cast_grave_claw(actor& caster, coord_def targ, int pow, bool fail);
void gain_grave_claw_soul(bool silent = false, bool wizard = false);

spret cast_fortress_blast(actor& caster, int pow, bool fail);
void unleash_fortress_blast(actor& caster);
dice_def fortress_blast_damage(int AC, bool is_monster);
