#ifndef SHOUT_H
#define SHOUT_H

#include "noise.h"

bool noisy(int loudness, const coord_def& where, mid_t who,
           noise_flag_type flags = NF_NONE);
bool noisy(int loudness, const coord_def& where, const char *msg = nullptr,
           mid_t who = MID_NOBODY, noise_flag_type flags = NF_NONE,
           bool fake_noise = false);
bool fake_noisy(int loudness, const coord_def& where);

void yell(const actor* mon = nullptr);

void item_noise(const item_def& item, string msg, int loudness = 25);
void noisy_equipment();

void check_monsters_sense(sense_type sense, int range, const coord_def& where);

void blood_smell(int strength, const coord_def& where);
void handle_monster_shouts(monster* mons, bool force = false);
bool check_awaken(monster* mons, int stealth);

void apply_noises();

#endif
