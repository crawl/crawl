/**
 * @file
 * @brief Misc functions.
**/

#ifndef MISC_H
#define MISC_H

#include "coord.h"
#include "target.h"

#include <algorithm>
#include <chrono>

string weird_glowing_colour();

string weird_writing();

string weird_smell();

string weird_sound();

bool bad_attack(const monster *mon, string& adj, string& suffix,
                bool& would_cause_penance,
                coord_def attack_pos = coord_def(0, 0),
                bool check_landing_only = false);

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool *prompted = nullptr,
                        coord_def attack_pos = coord_def(0, 0),
                        bool check_landing_only = false);

bool stop_attack_prompt(targetter &hitfunc, const char* verb,
                        bool (*affects)(const actor *victim) = 0,
                        bool *prompted = nullptr);

void swap_with_monster(monster *mon_to_swap);

void handle_real_time(chrono::time_point<chrono::system_clock> when
                      = chrono::system_clock::now());

bool today_is_halloween();

unsigned int breakpoint_rank(int val, const int breakpoints[],
                             unsigned int num_breakpoints);

bool tobool(maybe_bool mb, bool def);
maybe_bool frombool(bool b);

struct counted_monster_list
{
    typedef pair<const monster* ,int> counted_monster;
    typedef vector<counted_monster> counted_list;
    counted_list list;
    void add(const monster* mons);
    int count();
    bool empty() { return list.empty(); }
    string describe(description_level_type desc = DESC_THE,
                    bool force_article = false);
};
#endif
