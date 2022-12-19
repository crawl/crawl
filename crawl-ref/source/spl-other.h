#pragma once

#include <vector>

#include "externs.h"
#include "god-type.h"
#include "spl-cast.h"

using std::vector;

#define ANIMATE_DEAD_POWER_KEY "animate_dead_power"

class actor;

spret cast_sublimation_of_blood(int pow, bool fail);
spret cast_death_channel(int pow, god_type god, bool fail);
spret cast_animate_dead(int pow, bool fail);

enum class recall_t
{
    yred,
    beogh,
};

struct passwall_path
{
    passwall_path(const actor &act, const coord_def& dir, const int max_range);
    coord_def start;
    coord_def delta;
    int range;
    bool dest_found;

    coord_def actual_dest;
    vector<coord_def> path;

    bool spell_succeeds() const;
    int max_walls() const;
    int actual_walls() const;
    bool is_valid(string *fail_msg) const;
    bool check_moveto() const;
    vector <coord_def> possible_dests() const;
};

void start_recall(recall_t type);
void recall_orders(monster *mons);
bool try_recall(mid_t mid);
void do_recall(int time);
void end_recall();

bool passwall_simplified_check(const actor &act);
spret cast_passwall(const coord_def& delta, int pow, bool fail);
spret cast_intoxicate(int pow, bool fail, bool tracer = false);
