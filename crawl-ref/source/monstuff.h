/*
 *  File:       monstuff.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef MONSTUFF_H
#define MONSTUFF_H

#include "mon-util.h"

enum mon_dam_level_type
{
    MDAM_OKAY,
    MDAM_LIGHTLY_DAMAGED,
    MDAM_MODERATELY_DAMAGED,
    MDAM_HEAVILY_DAMAGED,
    MDAM_SEVERELY_DAMAGED,
    MDAM_ALMOST_DEAD,
    MDAM_DEAD
};

enum mon_desc_type   // things that cross categorical lines {dlb}
{
    MDSC_LEAVES_HIDE,                  //    0
    MDSC_REGENERATES,
    MDSC_NOMSG_WOUNDS
};

struct level_exit
{
    coord_def target;
    bool unreachable;

public:
    level_exit(coord_def t = coord_def(-1, -1),
               bool u = true)

        : target(t), unreachable(u)
    {
    }
};

#define FRESHEST_CORPSE 210

#define YOU_KILL(x) ((x) == KILL_YOU || (x) == KILL_YOU_MISSILE \
                     || (x) == KILL_YOU_CONF)
#define MON_KILL(x) ((x) == KILL_MON || (x) == KILL_MON_MISSILE)

// useful macro
#define SAME_ATTITUDE(x) (mons_friendly_real(x) ? BEH_FRIENDLY : \
                          mons_good_neutral(x)  ? BEH_GOOD_NEUTRAL : \
                          mons_neutral(x)       ? BEH_NEUTRAL \
                                                : BEH_HOSTILE)

#define MONST_INTERESTING(x) (x->flags & MF_INTERESTING)

// for definition of type monsters {dlb}
#include "externs.h"
// for definition of type monsters {dlb}

void get_mimic_item( const monsters *mimic, item_def & item );
int  get_mimic_colour( const monsters *mimic );

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
   * called from: fight - item_use - items - spell
   * *********************************************************************** */
void alert_nearby_monsters(void);

enum poly_power_type {
    PPT_LESS,
    PPT_MORE,
    PPT_SAME
};

bool monster_polymorph(monsters *monster, monster_type targetc,
                       poly_power_type power = PPT_SAME,
                       bool force_beh = false);

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
   * called from: bang - beam - effects - fight - misc - monstuff - mstuff2 -
   *              spells1 - spells2 - spells3 - spells4
   * *********************************************************************** */
int monster_die(monsters *monster, killer_type killer,
                int killer_index, bool silent = false, bool wizard = false);

int fill_out_corpse(const monsters* monster, item_def& corpse,
                    bool allow_weightless = false);

int place_monster_corpse(const monsters *monster, bool silent,
                         bool force = false);

void mons_check_pool(monsters *monster, const coord_def &oldpos,
                     killer_type killer = KILL_NONE, int killnum = -1);

// last updated: 17dec2000 {gdl}
/* ***********************************************************************
   * called from: monstuff - fight
   * *********************************************************************** */
void monster_cleanup(monsters *monster);


void behaviour_event(monsters *mon, mon_event_type event_type,
                     int src = MHITNOT, coord_def src_pos = coord_def(),
                     bool allow_shout = true);

bool curse_an_item(bool decay_potions, bool quiet = false);


void monster_drop_ething(monsters *monster, bool mark_item_origins = false,
                         int owner_id = NON_ITEM);

/* ***********************************************************************
 * called from: fight
 * *********************************************************************** */
bool monster_blink(monsters *monster, bool quiet = false);


/* ***********************************************************************
 * called from: spells1 spells4 monstuff
 * defaults are set up for player blink;  monster blink should call with
 * false, false
 * *********************************************************************** */
bool random_near_space(const coord_def& origin, coord_def& target,
                       bool allow_adjacent = false, bool restrict_LOS = true,
                       bool forbid_sanctuary = false);


/* ***********************************************************************
 * called from: beam - effects - fight - monstuff - mstuff2 - spells1 -
 *              spells2 - spells4
 * *********************************************************************** */
bool simple_monster_message(const monsters *monster, const char *event,
                            msg_channel_type channel = MSGCH_PLAIN,
                            int param = 0,
                            description_level_type descrip = DESC_CAP_THE);

void make_mons_leave_level(monsters *mon);

bool choose_any_monster(const monsters* mon);
monsters *choose_random_nearby_monster(
    int weight,
    bool (*suitable)(const monsters* mon) =
        choose_any_monster,
    bool in_sight = true,
    bool prefer_named = false, bool prefer_priest = false);

monsters *choose_random_monster_on_level(
    int weight,
    bool (*suitable)(const monsters* mon) =
        choose_any_monster,
    bool in_sight = true, bool near_by = false,
    bool prefer_named = false, bool prefer_priest = false);

bool swap_places(monsters *monster);
bool swap_places(monsters *monster, const coord_def &loc);
bool swap_check(monsters *monster, coord_def &loc, bool quiet = false);


std::string get_wounds_description(const monsters *monster);
void print_wounds(const monsters *monster);
void handle_monsters(void);
bool monster_descriptor(int which_class, mon_desc_type which_descriptor);
bool message_current_target(void);

unsigned int monster_index(const monsters *monster);

void mons_get_damage_level(const monsters*, std::string& desc,
                           mon_dam_level_type&);

bool heal_monster(monsters *patient, int health_boost, bool permit_growth);

void seen_monster(monsters *monster);

bool shift_monster(monsters *mon, coord_def p = coord_def(0, 0));

int mons_weapon_damage_rating(const item_def &launcher);
int mons_missile_damage(monsters *mons, const item_def *launch,
                        const item_def *missile);
int mons_pick_best_missile(monsters *mons, item_def **launcher,
                           bool ignore_melee = false);
int mons_thrown_weapon_damage(const item_def *weap);

int mons_natural_regen_rate(monsters *monster);

bool mons_avoids_cloud(const monsters *monster, cloud_type cl_type,
                       bool placement = false);

// Like the above, but allow a monster to move from one damaging cloud
// to another.
bool mons_avoids_cloud(const monsters *monster, int cloud_num,
                       cloud_type *cl_type = NULL, bool placement = false);
#endif
