/**
 * @file
 * @brief Misc Xom related functions.
**/

#pragma once

#include <string>

#include "kill-method-type.h"
#include "maybe-bool.h"
#include "monster.h"

using std::string;

#define XOM_CLOUD_TRAIL_TYPE_KEY "xom_cloud_trail_type"
#define XOM_BAZAAR_TRIP_COUNT "xom_bazaar_trip_count"

struct item_def;

enum xom_message_type
{
    XM_NORMAL,
    XM_INTRIGUED,
    NUM_XOM_MESSAGE_TYPES
};

enum xom_event_type
{
    XOM_DID_NOTHING = 0,

    // good acts
    XOM_GOOD_POTION,
    XOM_GOOD_DIVINATION,
    XOM_GOOD_SPELL,
    XOM_GOOD_CONFUSION,
    XOM_GOOD_SINGLE_ALLY,
    XOM_GOOD_ANIMATE_MON_WPN,
    XOM_GOOD_RANDOM_ITEM,
    XOM_GOOD_ACQUIREMENT,
    XOM_GOOD_BAZAAR_TRIP,
    XOM_GOOD_ALLIES,
    XOM_GOOD_POLYMORPH,
    XOM_GOOD_TELEPORT,
    XOM_GOOD_MUTATION,
    XOM_GOOD_LIGHTNING,
    XOM_GOOD_SCENERY,
    XOM_GOOD_FLORA_RING,
    XOM_GOOD_DOOR_RING,
    XOM_GOOD_SNAKES,
    XOM_GOOD_LIGHT_UP_WEBS,
    XOM_GOOD_DESTRUCTION,
    XOM_GOOD_FAKE_DESTRUCTION,
    XOM_GOOD_FORCE_LANCE_FLEET,
    XOM_GOOD_ENCHANT_MONSTER,
    XOM_GOOD_HYPER_ENCHANT_MONSTER,
    XOM_GOOD_MASS_CHARM,
    XOM_GOOD_WAVE_OF_DESPAIR,
    XOM_GOOD_FOG,
    XOM_GOOD_CLOUD_TRAIL,
    XOM_GOOD_CLEAVING,
    XOM_LAST_GOOD_ACT = XOM_GOOD_CLEAVING,

    // bad acts
    XOM_BAD_MISCAST_PSEUDO,
    XOM_BAD_TELEPORT,
    XOM_BAD_CHAOS_UPGRADE,
    XOM_BAD_MUTATION,
    XOM_BAD_POLYMORPH,
    XOM_BAD_MOVING_STAIRS,
    XOM_BAD_CLIMB_STAIRS,
    XOM_BAD_FIDDLE_WITH_DOORS,
    XOM_BAD_DOOR_RING,
    XOM_BAD_FAKE_SHATTER,
    XOM_BAD_CONFUSION,
    XOM_BAD_DRAINING,
    XOM_BAD_TORMENT,
    XOM_BAD_BRAIN_DRAIN,
    XOM_BAD_SUMMON_HOSTILES,
    XOM_BAD_SEND_IN_THE_CLONES,
    XOM_BAD_GRANT_WORD_OF_RECALL,
    XOM_BAD_PSEUDO_BANISHMENT,
    XOM_BAD_BANISHMENT,
    XOM_BAD_NOISE,
    XOM_BAD_ENCHANT_MONSTER,
    XOM_BAD_TIME_CONTROL,
    XOM_BAD_BLINK_MONSTERS,
    XOM_BAD_CHAOS_CLOUD,
    XOM_BAD_SWAP_MONSTERS,
    XOM_LAST_BAD_ACT = XOM_BAD_SWAP_MONSTERS,
    XOM_LAST_REAL_ACT = XOM_LAST_BAD_ACT,

    XOM_PLAYER_DEAD = 100, // player already dead (shouldn't happen)
    NUM_XOM_EVENTS
};

void xom_tick();
void xom_is_stimulated(int maxinterestingness,
                       xom_message_type message_type = XM_NORMAL,
                       bool force_message = false);
void xom_is_stimulated(int maxinterestingness, const string& message,
                       bool force_message = false);
bool xom_is_nice(int tension = -1);
const string describe_xom_favour();
int xom_favour_rank();

xom_event_type xom_acts(int sever, maybe_bool niceness = maybe_bool::maybe,
                        int tension = -1, bool debug = false);
xom_event_type xom_choose_action(bool niceness,  int sever, int tension);
void xom_take_action(xom_event_type action, int sever);

xom_event_type xom_maybe_reverts_banishment(bool xom_banished = true,
                                            bool debug = false);
void xom_check_lost_item(const item_def& item);
void xom_check_destroyed_item(const item_def& item);
void xom_death_message(const kill_method_type killed_by);
bool xom_saves_your_life(const kill_method_type death_type);
void xom_new_level_noise_or_stealth();

string xom_effect_to_name(xom_event_type effect);

#ifdef WIZARD
void debug_xom_effects();
#endif

bool swap_monsters(monster* m1, monster* m2);
bool move_stair(coord_def stair_pos, bool away, bool allow_under);

void validate_xom_events();
