#pragma once

#include <vector>

#include "externs.h"
#include "god-type.h"
#include "spl-cast.h"

using std::vector;

#define ANIMATE_DEAD_POWER_KEY "animate_dead_power"

#define SPIKE_LAUNCHER_TIMER "spike_launcher_timer"
#define SPIKE_LAUNCHER_POWER "spike_launcher_power"

class actor;

spret cast_sublimation_of_blood(int pow, bool fail);
spret cast_death_channel(int pow, god_type god, bool fail);
spret cast_animate_dead(int pow, bool fail);

vector<coord_def> find_sigil_locations(bool tracer);
spret cast_sigil_of_binding(int pow, bool fail, bool tracer);
void trigger_binding_sigil(actor& actor);

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

void do_player_recall(recall_t type);
void recall_orders(monster *mons);
bool try_recall(mid_t mid);
void do_player_recall(int time);
void end_recall();

bool passwall_simplified_check(const actor &act);
spret cast_passwall(const coord_def& delta, int pow, bool fail);
spret cast_intoxicate(int pow, bool fail, bool tracer = false);

vector<coord_def> find_spike_launcher_walls();
spret cast_spike_launcher(int pow, bool fail);
void handle_spike_launcher(int delay);
void end_spike_launcher();
