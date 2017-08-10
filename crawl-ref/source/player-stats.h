#pragma once

#include "duration-type.h"
#include "spl-cast.h"
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

void jiyva_stat_action();

int stat_loss_roll();
bool lose_stat(stat_type which_stat, int stat_loss, bool force = false);

stat_type random_lost_stat();
bool restore_stat(stat_type which_stat, int stat_gain,
                  bool suppress_msg, bool recovery = false);

duration_type stat_zero_duration(stat_type stat);
bool have_stat_zero();
void update_stat_zero(int time);

spret_type gnoll_shift_attributes();

int innate_stat(stat_type s);
