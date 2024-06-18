#pragma once

#include "spl-cast.h"

class actor;
class dist;
struct dice_def;

const int GOLUBRIA_FUZZ_RANGE = 2;
const int GAVOTTE_DISTANCE = 4;

spret cast_disjunction(int pow, bool fail);
void disjunction_spell();

spret cast_blink(int pow, bool fail = false);
void uncontrolled_blink(bool override_stasis = false);
spret controlled_blink(bool safe_cancel = true, dist *target = nullptr);
void wizard_blink();

int frog_hop_range();
spret frog_hop(bool fail, dist *target = nullptr);

string electric_charge_impossible_reason(bool allow_safe_monsters);
spret electric_charge(actor& agent, int powc, bool fail, const coord_def &target);
bool find_charge_target(vector<coord_def> &target_path, int max_range,
                                targeter *hitfunc, dist &target);
string movement_impossible_reason();

coord_def get_electric_charge_landing_spot(const actor& agent,
                                           coord_def target,
                                           string* fail_reason = nullptr);

bool valid_electric_charge_target(const actor& agent, coord_def target,
                                  string* fail_reason = nullptr);

void you_teleport();
void you_teleport_now(bool wizard_tele = false, bool teleportitis = false,
                      string reason = "");
bool you_teleport_to(const coord_def where,
                     bool move_monsters = false);
bool cell_vetoes_teleport(coord_def cell, bool check_monsters = true,
                          bool wizard_tele = false);

spret cast_dimensional_bullseye(int pow, monster *target, bool fail);

spret cast_manifold_assault(actor& agent, int pow, bool fail, bool real = true,
                            actor* katana_defender = nullptr);
string weapon_unprojectability_reason(const item_def* wpn);

struct bolt;
spret cast_apportation(int pow, bolt& beam, bool fail);
bool golubria_valid_cell(coord_def p, bool just_check = false);
spret cast_golubrias_passage(int pow, const coord_def& where, bool fail);

spret cast_dispersal(int pow, bool fail);

int gravitas_radius(int pow);
spret cast_gravitas(int pow, const coord_def& where, bool fail);

bool beckon(actor &beckoned, const bolt &path);
void attract_monsters(int delay);
void attract_monster(monster &m, int max_move);
vector<monster *> find_chaos_targets(bool just_check = false);
spret word_of_chaos(int pow, bool fail);
spret blinkbolt(int power, bolt &beam, bool fail);

dice_def piledriver_collision_damage(int pow, int dist, bool random);
vector<coord_def> piledriver_beam_paths(const vector<coord_def> &targets);
vector<coord_def> possible_piledriver_targets();
spret cast_piledriver(int pow, bool fail);

dice_def gavotte_impact_damage(int pow, int dist, bool random);
spret cast_gavotte(int pow, const coord_def dir, bool fail);
vector<monster*> gavotte_affected_monsters(const coord_def dir,
                                           bool actual = true);
