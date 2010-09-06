#ifndef SPL_CLOUDS_H
#define SPL_CLOUDS_H

struct bolt;

bool conjure_flame(int pow, const coord_def& where);
bool stinking_cloud(int pow, bolt &beam);

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

void cast_ring_of_flames(int power);
void manage_fire_shield(int delay);

void corpse_rot();

int make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                        cloud_type ctype, kill_category,
                        killer_type killer = KILL_NONE, int colour = -1,
                        std::string name = "", std::string tile = "");

std::string get_evaporate_result_list(int potion);
bool cast_evaporate(int pow, bolt& beem, int potion);

int holy_flames (monsters* caster, actor* defender);
#endif
