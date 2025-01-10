#pragma once

#include "duration-type.h"
#include "mutation-type.h"
#include "stat-type.h"

enum stat_desc_type
{
    SD_NAME,
    SD_LOSS,
    SD_DECREASE,
    SD_INCREASE,
    NUM_STAT_DESCS
};

const char* stat_desc(stat_type stat, stat_desc_type desc);

bool attribute_increase();

void modify_stat(stat_type which_stat, int amount, bool suppress_msg);

void notify_stat_change(stat_type which_stat, int amount, bool suppress_msg);
void notify_stat_change();

bool have_stat_zero();
void update_stat_zero(int time);
bool mutation_causes_stat_zero(mutation_type mut);

int innate_stat(stat_type s);
