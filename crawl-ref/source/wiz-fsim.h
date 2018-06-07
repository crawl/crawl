/**
 * @file
 * @brief Fight simulation wizard functions.
**/

#pragma once

struct fight_damage_stats
{
    fight_damage_stats(string att) : cumulative_damage(0), time_taken(0), hits(0),
            iterations(1), attacker(att),
            av_hit_dam(0.0), max_dam(0), accuracy(0), av_dam(0.0), av_time(0),
            av_speed(0.0), av_eff_dam(0.0)
    {};

    void calc_output_stats();
    void damage(int amount);

    string summary(const string prefix, bool tsv);

    // used while running an fsim
    unsigned int cumulative_damage;
    int time_taken;
    int hits;
    int iterations;

    string attacker;

    // output stats
    double av_hit_dam;
    int max_dam;
    int accuracy;
    double av_dam;
    int av_time;
    double av_speed;
    double av_eff_dam;
};

struct fight_data
{
    fight_data() : player("Player"), monster("Mons")
    {};

    static string header(bool tsv);
    string summary(const string prefix, bool tsv);

    fight_damage_stats player;
    fight_damage_stats monster;
};

void wizard_quick_fsim();
void wizard_fight_sim(bool double_scale);
fight_data wizard_quick_fsim_raw(bool defend);
