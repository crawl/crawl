/**
 * @file
 * @brief Tracking permaallies granted by Yred and Beogh.
**/

#pragma once

#include <list>
#include <map>
#include <vector>

#include "monster.h"
#include "mon-transit.h"
#include "tag-version.h"

using std::vector;

struct companion
{
    follower mons;
    level_id level;
    int timestamp;

    companion() : mons(), level() { }
    companion(const monster& m);
};

extern map<mid_t, companion> companion_list;

#define BEOGH_CHALLENGE_PROGRESS_KEY "beogh_challenge_progress"
#define BEOGH_RECRUITABLE_APOSTLE_KEY "beogh_recruitable_apostle"
#define BEOGH_SAVED_APOSTLES_KEY "beogh_saved_apostles"
#define BEOGH_RECRUITABLE_APOSTLE_DEATH_POS_KEY "beogh_recruitable_death_pos"
#define BEOGH_NUM_FOLLOWERS_KEY "beogh_num_followers"
#define BEOGH_RES_PIETY_NEEDED_KEY "beogh_resurrection_piety_needed"
#define BEOGH_RES_PIETY_GAINED_KEY "beogh_resurrection_piety_gained"
#define BEOGH_VENGEANCE_NUM_KEY "beogh_vengeance_num"
#define BEOGH_APOSTLE_DEATH_FLOOR_PREFIX "beogh_apostle_death_floor_"
#define BEOGH_BFB_VALID_KEY "beogh_bfb_valid"
#define BEOGH_FOLLOWER_DEATH_PREFIX "beogh_apostle_banished_"

void init_companions();
void add_companion(monster* mons);
void remove_companion(monster* mons);
void remove_bound_soul_companion();
void remove_all_companions(god_type god);
void move_companion_to(const monster* mons, const level_id lid);

void update_companions();

bool companion_is_elsewhere(mid_t mid, bool must_exist = false);

void populate_offlevel_recall_list(vector<pair<mid_t, int> > &recall_list);
bool recall_offlevel_ally(mid_t mid);

void wizard_list_companions();

mid_t hepliaklqana_ancestor();
monster* hepliaklqana_ancestor_mon();
bool ancestor_full_hp();

bool maybe_generate_apostle_challenge();
void flee_apostle_challenge();
void win_apostle_challenge(monster& apostle);
void end_beogh_recruit_window();

void beogh_do_ostracism();
void beogh_end_ostracism();

void beogh_recruit_apostle();
void beogh_dismiss_apostle(int slot);
string get_apostle_name(int slot, bool with_title = false);

void beogh_swear_vegeance(monster& apostle);
void beogh_follower_banished(monster& apostle);
void beogh_progress_vengeance();
void beogh_progress_resurrection(int amount);
void beogh_resurrect_followers(bool end_ostracism_only = false);

void beogh_note_follower_death(const monster& apostle);

bool tile_has_valid_bfb_corpse(const coord_def pos);

#if TAG_MAJOR_VERSION == 34
void fixup_bad_companions();
bool maybe_bad_priest_monster(monster &mons);
void fixup_bad_priest_monster(monster &mons);
#endif
