/**
 * @file
 * @brief Fight simulation wizard functions.
**/

#ifndef WIZFSIM_H
#define WIZFSIM_H

struct fight_data {
double av_hit_dam;
int max_dam;
int accuracy;
double av_dam;
double av_time;
double av_eff_dam;
};

void wiz_run_fight_sim(monster_type mtype = MONS_WORM,
                        int iter_limit = 5000);
void wiz_fight_sim_file(bool defend = false,
                        monster_type mtype = MONS_WORM,
                        int iter_limit = 5000,
                        const char * fightstat = "fight.stat");
void skill_vs_fighting(int iter_limit = 5000);

#endif
