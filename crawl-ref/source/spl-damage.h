#ifndef SPL_DAMAGE_H
#define SPL_DAMAGE_H

#include "enum.h"

struct bolt;
class dist;

bool fireball(int pow, bolt &beam);
void setup_fire_storm(const actor *source, int pow, bolt &beam);
bool cast_fire_storm(int pow, bolt &beam);
bool cast_hellfire_burst(int pow, bolt &beam);
void cast_chain_lightning(int pow, const actor *caster);

void cast_toxic_radiance(bool non_player = false);
void cast_refrigeration(int pow, bool non_player = false);
bool vampiric_drain(int pow, monsters *monster);
bool burn_freeze(int pow, beam_type flavour, monsters *monster);

int airstrike(int pow, const dist &beam);
bool cast_bone_shards(int power, bolt &);

void cast_shatter(int pow);
void cast_ignite_poison(int pow);
void cast_discharge(int pow);
int disperse_monsters(coord_def where, int pow);
void cast_dispersal(int pow);
bool cast_fragmentation(int powc, const dist& spd);
bool wielding_rocks();
bool cast_sandblast(int powc, bolt &beam);

#endif
