/**
 * @file
 * @brief Gametime related functions.
**/

#ifndef TIME_H
#define TIME_H

void change_labyrinth(bool msg = false);

void update_level(int elapsedTime);
void handle_time();
void recharge_rods(int aut, bool floor_only);

void timeout_tombs(int duration);

int count_malign_gateways();
void timeout_malign_gateways(int duration);

void timeout_terrain_changes(int duration, bool force = false);

void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();
int speed_to_duration(int speed);

enum timed_effect_type
{
    TIMER_CORPSES,
    TIMER_HELL_EFFECTS,
    TIMER_STAT_RECOVERY,
    TIMER_CONTAM,
    TIMER_DETERIORATION,
    TIMER_GOD_EFFECTS,
#if TAG_MAJOR_VERSION == 34
    TIMER_SCREAM,
#endif
    TIMER_FOOD_ROT,
    TIMER_PRACTICE,
    TIMER_LABYRINTH,
    TIMER_ABYSS_SPEED,
    TIMER_JIYVA,
    TIMER_EVOLUTION,
#if TAG_MAJOR_VERSION == 34
    TIMER_BRIBE_TIMEOUT,
#endif
    NUM_TIMERS,
};

#endif
