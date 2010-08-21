/*
 *  File:       spells1.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */


#ifndef SPELLS1_H
#define SPELLS1_H

struct bolt;

bool cast_sure_blade(int power);
int cast_healing(int pow, bool divine_ability = false,
                 const coord_def& where = coord_def(0, 0),
                 bool not_self = false, targ_mode_type mode = TARG_NUM_MODES);

void big_cloud(cloud_type cl_type, kill_category whose, const coord_def& where,
               int pow, int size, int spread_rate = -1, int colour = -1,
               std::string name = "", std::string tile = "");
void big_cloud(cloud_type cl_type, killer_type killer, const coord_def& where,
               int pow, int size, int spread_rate = -1, int colour = -1,
               std::string name = "", std::string tile = "");
void big_cloud(cloud_type cl_type, kill_category whose, killer_type killer,
               const coord_def& where, int pow, int size, int spread_rate = -1,
               int colour = -1, std::string name = "", std::string tile = "");

int cast_big_c(int pow, cloud_type cty, kill_category whose, bolt &beam);
void cast_confusing_touch(int power);
void cast_cure_poison(int pow);
int allowed_deaths_door_hp(void);
void cast_deaths_door(int pow);
void setup_fire_storm(const actor *source, int pow, bolt &beam);
bool cast_fire_storm(int pow, bolt &beam);
bool cast_hellfire_burst(int pow, bolt &beam);
void cast_chain_lightning(int pow, const actor *caster);
bool cast_revivification(int pow);
void cast_berserk(void);
void cast_ring_of_flames(int power);
bool conjure_flame(int pow, const coord_def& where);
void extension(int pow);
bool fireball(int pow, bolt &beam);
bool stinking_cloud(int pow, bolt &beam);
void abjuration(int pow);
void cast_fly(int power);

void cast_insulation(int power);
void remove_regen(bool divine_ability = false);
void cast_regen(int pow, bool divine_ability = false);
void cast_resist_poison(int power);
void cast_swiftness(int power);
void cast_teleport_control(int power);
void deflection(int pow);
void remove_ice_armour();
void ice_armour(int pow, bool extending);
void missile_prot(int pow);
void stone_scales(int pow);

void antimagic();
void identify(int power, int item_slot = -1);
void manage_fire_shield(int delay);
#endif
