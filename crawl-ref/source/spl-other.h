#pragma once

#include "god-type.h"
#include "spl-cast.h"

spret_type cast_sublimation_of_blood(int pow, bool fail);
spret_type cast_death_channel(int pow, god_type god, bool fail);

enum recall_t
{
    RECALL_SPELL,
    RECALL_YRED,
    RECALL_BEOGH,
};

spret_type cast_recall(bool fail);
void start_recall(recall_t type);
void recall_orders(monster *mons);
bool try_recall(mid_t mid);
void do_recall(int time);
void end_recall();

spret_type cast_passwall(const coord_def& delta, int pow, bool fail);
spret_type cast_intoxicate(int pow, bool fail);
spret_type cast_darkness(int pow, bool fail);
