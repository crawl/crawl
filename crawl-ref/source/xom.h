/*
 *  File:       xom.h
 *  Summary:    Misc Xom related functions.
 *  Written by: Linley Henzell
 */

#ifndef XOM_H
#define XOM_H

#include "ouch.h"

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
    XOM_GOOD_NOTHING, // good act suppressed
    XOM_GOOD_POTION,
    XOM_GOOD_SPELL_TENSION,
    XOM_GOOD_SPELL_CALM,
    XOM_GOOD_MAPPING,           //   5
    XOM_GOOD_CONFUSION,
    XOM_GOOD_SINGLE_ALLY,
    XOM_GOOD_ANIMATE_MON_WPN,
    XOM_GOOD_ANNOYANCE_GIFT,
    XOM_GOOD_RANDOM_ITEM,       //  10
    XOM_GOOD_ACQUIREMENT,
    XOM_GOOD_ALLIES,
    XOM_GOOD_POLYMORPH,
    XOM_GOOD_SWAP_MONSTERS,
    XOM_GOOD_TELEPORT,          //  15
    XOM_GOOD_VITRIFY,
    XOM_GOOD_MUTATION,
    XOM_GOOD_MAJOR_ALLY,
    XOM_GOOD_LIGHTNING,
    XOM_GOOD_SCENERY,           //  20
    XOM_GOOD_SNAKES,
    XOM_LAST_GOOD_ACT = XOM_GOOD_SNAKES,   // 21

    // bad acts
    XOM_BAD_NOTHING,  // bad act suppressed
    XOM_BAD_COLOUR_SMOKE_TRAIL,
    XOM_BAD_MISCAST_PSEUDO,
    XOM_BAD_MISCAST_MINOR,
    XOM_BAD_MISCAST_MAJOR,      //  25
    XOM_BAD_MISCAST_NASTY,
    XOM_BAD_STATLOSS,
    XOM_BAD_TELEPORT,
    XOM_BAD_SWAP_WEAPONS,
    XOM_BAD_CHAOS_UPGRADE,      //  30
    XOM_BAD_MUTATION,
    XOM_BAD_POLYMORPH,
    XOM_BAD_STAIRS,
    XOM_BAD_CONFUSION,
    XOM_BAD_DRAINING,           //  35
    XOM_BAD_TORMENT,
    XOM_BAD_ANIMATE_WPN,
    XOM_BAD_SUMMON_DEMONS,
    XOM_BAD_PSEUDO_BANISHMENT,  //  39
    XOM_BAD_BANISHMENT,
    XOM_LAST_BAD_ACT = XOM_BAD_BANISHMENT,  // 40

    XOM_PLAYER_DEAD = 100, // player already dead (shouldn't happen)
    NUM_XOM_EVENTS
};

void xom_tick();
void xom_is_stimulated(int maxinterestingness,
                       xom_message_type message_type = XM_NORMAL,
                       bool force_message = false);
void xom_is_stimulated(int maxinterestingness, const std::string& message,
                       bool force_message = false);
bool xom_is_nice(int tension = -1);
int xom_acts(bool niceness, int sever, int tension = -1, bool debug = false);
const std::string describe_xom_favour(bool upper = false);

inline int xom_acts(int sever, int tension = -1)
{
    return xom_acts(xom_is_nice(tension), sever, tension);
}

void xom_check_lost_item(const item_def& item);
void xom_check_destroyed_item(const item_def& item, int cause = -1);
void xom_death_message(const kill_method_type killed_by);
bool xom_saves_your_life(const int dam, const int death_source,
                         const kill_method_type death_type, const char *aux,
                         const bool see_source);

#ifdef WIZARD
void debug_xom_effects();
#endif

bool move_stair(coord_def stair_pos, bool away, bool allow_under);

#endif
