#pragma once

#include "mon-enum.h"
#include "noise.h"

bool noisy(int loudness, const coord_def& where, mid_t who);
bool noisy(int loudness, const coord_def& where, const char *msg = nullptr,
           mid_t who = MID_NOBODY, bool fake_noise = false);
bool fake_noisy(int loudness, const coord_def& where);

void yell(const actor* target = nullptr);
void issue_orders();

void item_noise(const item_def& item, actor &act, string msg, int loudness = 25);
void noisy_equipment(const item_def &item);

void monster_consider_shouting(monster &mon);
bool monster_attempt_shout(monster &mon);
void monster_shout(monster &mons, int s_type);
int monster_perception(monster* mons);
int monster_perception(int HD, mon_intel_type intel, bool is_asleep);
bool check_awaken(monster* mons, int stealth);

void apply_noises();
