#ifndef SPL_CLOUDS_H
#define SPL_CLOUDS_H

struct bolt;

bool conjure_flame(int pow, const coord_def& where);
bool stinking_cloud(int pow, bolt &beam);

void big_cloud(cloud_type cl_type, const actor *agent, const coord_def& where,
               int pow, int size, int spread_rate = -1, int colour = -1,
               std::string name = "", std::string tile = "");

bool cast_big_c(int pow, cloud_type cty, const actor *caster, bolt &beam);

void cast_ring_of_flames(int power);
void manage_fire_shield(int delay);

void corpse_rot(actor* caster);

int make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                        cloud_type ctype, const actor *agent, int colour = -1,
                        std::string name = "", std::string tile = "");

std::string get_evaporate_result_list(int potion);
bool cast_evaporate(int pow, bolt& beem, int potion);

int holy_flames(monster* caster, actor* defender);
#endif
