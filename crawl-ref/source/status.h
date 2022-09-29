/**
 * @file
 * @brief Collects information for output of status effects.
**/

#pragma once

#include "duration-type.h"
#include "mpr.h"

enum status_type
{
    STATUS_AIRBORNE = NUM_DURATIONS + 1,
    STATUS_BEHELD,
    STATUS_CONTAMINATION,
    STATUS_NET,
    STATUS_ALIVE_STATE,
    STATUS_REGENERATION,
    STATUS_DIG,
    STATUS_SPEED,
    STATUS_STR_ZERO,
    STATUS_INT_ZERO,
    STATUS_DEX_ZERO,
    STATUS_BACKLIT,
    STATUS_UMBRA,
    STATUS_CONSTRICTED,
    STATUS_MANUAL,
    STATUS_AUGMENTED,
    STATUS_TERRAIN,
    STATUS_SILENCE,
    STATUS_BEOGH,
    STATUS_RECALL,
    STATUS_LIQUEFIED,
    STATUS_DRAINED,
    STATUS_RAY,
    STATUS_INVISIBLE,
    STATUS_BRIBE,
    STATUS_CLOUD,
    STATUS_ORB,
    STATUS_STILL_WINDS,
    STATUS_MISSILES,
    STATUS_SERPENTS_LASH,
    STATUS_HEAVENLY_STORM,
    STATUS_ZOT,
    STATUS_MAXWELLS,
    STATUS_DUEL,
    STATUS_NO_SCROLL,
    STATUS_RF_ZERO,
    STATUS_CORROSION,
    STATUS_NO_POTIONS,
    STATUS_LOWERED_WL,
    STATUS_FLAME_WAVE,
    STATUS_LAST_STATUS = STATUS_FLAME_WAVE
};

struct status_info
{
    status_info() : light_colour(0)
    {
    };

    int light_colour;
    string light_text; // status light
    string short_text; // @: line
    string long_text;  // @ message
};

// status should be a duration or status_type
// *info will be filled in as appropriate for current
// character state
// returns true if the status has a description
bool fill_status_info(int status, status_info& info);

const char *duration_name(duration_type dur);
bool duration_dispellable(duration_type dur);
void init_duration_index();

bool duration_decrements_normally(duration_type dur);
const char *duration_end_message(duration_type dur);
void duration_end_effect(duration_type dur);
const char *duration_expire_message(duration_type dur);
int duration_expire_offset(duration_type dur);
int duration_expire_point(duration_type dur);
msg_channel_type duration_expire_chan(duration_type dur);
