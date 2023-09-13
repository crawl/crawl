/**
 * @file
 * @brief Misc abyss specific functions.
**/

#pragma once

// When shifting areas in the abyss, shift the square containing player LOS
// plus a little extra so that the player won't be disoriented by taking a
// step backward after an abyss shift.
const int ABYSS_AREA_SHIFT_RADIUS = LOS_RADIUS + 2;
const int ABYSSAL_RUNE_MIN_LEVEL = 3;
extern const coord_def ABYSS_CENTRE;

/// How much XP does the player need to earn to spawn the next exit/stair?
#define ABYSS_STAIR_XP_KEY "abyss_stair_xp" // not actually xp... complicated
/// Has the Abyss spawned an exit for the player due to earned XP in this trip?
#define ABYSS_SPAWNED_XP_EXIT_KEY "abyss_spawned_xp_exit"
const int EXIT_XP_COST = 10; // ref _reduce_abyss_xp_timer() for details
// but it's equivalent to roughly half the recharge xp for an elemental evoker
#define ABYSS_MIN_DEPTH_KEY "abyss_min_depth_key"

#define ABYSSAL_RUNE_LOC_KEY "abyssal_rune_loc"

struct abyss_state
{
    coord_def major_coord;
    uint32_t seed;
    uint32_t depth;
    double phase;
    level_id level;
    bool destroy_all_terrain;
};

extern abyss_state abyssal_state;

void abyss_morph();

void banished(const string &who = "", const int power = 0);
void push_features_to_abyss();

void clear_abyssal_rune_knowledge();
void generate_abyss();
void maybe_shift_abyss_around_player();
void abyss_maybe_spawn_xp_exit();
void abyss_teleport(bool wizard_tele = false);
void save_abyss_uniques();
bool is_level_incorruptible(bool quiet = false);
bool is_level_incorruptible_monster();
void lugonu_corrupt_level(int power);
void lugonu_corrupt_level_monster(const monster &who);
void run_corruption_effects(int duration);
void splash_corruption(coord_def centre);
void set_abyss_state(coord_def coord, uint32_t depth);
void destroy_abyss();
