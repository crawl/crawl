/**
 * @file
 * @brief Fight simulation wizard functions.
**/

#pragma once

struct fight_data
{
    double av_hit_dam;
    int max_dam;
    int accuracy;
    double av_dam;
    int av_time;
    double av_speed;
    double av_eff_dam;
};

void wizard_quick_fsim();
void wizard_fight_sim(bool double_scale);

