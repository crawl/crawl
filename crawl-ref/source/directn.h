/*
 *  File:       directn.h
 *  Summary:    Functions used when picking squares.
 *  Written by: Linley Henzell
 */


#ifndef DIRECT_H
#define DIRECT_H

#include "describe.h"
#include "externs.h"
#include "enum.h"
#include "ray.h"
#include "state.h"

class range_view_annotator
{
public:
    range_view_annotator(int range);
    virtual ~range_view_annotator();
};

// An object that modifies the behaviour of the direction prompt.
class targetting_behaviour
{
public:
    targetting_behaviour(bool just_looking = false);
    virtual ~targetting_behaviour();

    // Returns a keystroke for the prompt.
    virtual int get_key();
    virtual command_type get_command(int key = -1);
    virtual bool should_redraw();
    virtual void mark_ammo_nonchosen();

public:
    bool just_looking;
    bool compass;
};

// output from direction() function:
class dist
{
public:
    dist();

public:
    bool isValid;       // valid target chosen?
    bool isTarget;      // target (true), or direction (false)?
    bool isMe;          // selected self (convenience: target == you.pos())
    bool isEndpoint;    // Does the player want the attack to stop at target?
    bool isCancel;      // user cancelled (usually <ESC> key)
    bool choseRay;      // user wants a specific beam
    coord_def target;   // target x,y or logical extension of beam to map edge
    coord_def delta;    // delta x and y if direction - always -1,0,1
    ray_def ray;        // ray chosen if necessary

    // internal use - ignore
    int  prev_target;   // previous target
};

void direction( dist &moves, targetting_type restricts = DIR_NONE,
                targ_mode_type mode = TARG_ANY, int range = -1,
                bool just_looking = false, bool needs_path = true,
                bool may_target_monster = true, bool may_target_self = false,
                const char *prompt = NULL, targetting_behaviour *mod = NULL,
                bool cancel_at_self = false );

bool in_los_bounds(const coord_def& p);
bool in_viewport_bounds(int x, int y);
inline bool in_viewport_bounds(const coord_def& pos) {
  return in_viewport_bounds(pos.x, pos.y);
}
bool in_los(int x, int y);
bool in_los(const coord_def &pos);
bool in_vlos(int x, int y);
bool in_vlos(const coord_def &pos);

std::string thing_do_grammar(description_level_type dtype,
                             bool add_stop,
                             bool force_article,
                             std::string desc);

std::string get_terse_square_desc(const coord_def &gc);
void terse_describe_square(const coord_def &c, bool in_range = true);
void full_describe_square(const coord_def &c);
void get_square_desc(const coord_def &c, describe_info &inf,
                     bool examine_mons = false);

void describe_floor();
std::string get_monster_equipment_desc(const monsters *mon,
                                bool full_desc = true,
                                description_level_type mondtype = DESC_CAP_A,
                                bool print_attitude = false);

int dos_direction_unmunge(int doskey);

std::string feature_description(const coord_def& where, bool bloody = false,
                                description_level_type dtype = DESC_CAP_A,
                                bool add_stop = true, bool base_desc = false);
std::string raw_feature_description(dungeon_feature_type grid,
                                    trap_type tr = NUM_TRAPS,
                                    bool base_desc = false);
std::string feature_description(dungeon_feature_type grid,
                                trap_type trap = NUM_TRAPS, bool bloody = false,
                                description_level_type dtype = DESC_CAP_A,
                                bool add_stop = true, bool base_desc = false);

void set_feature_desc_short(dungeon_feature_type grid,
                            const std::string &desc);
void set_feature_desc_short(const std::string &base_name,
                            const std::string &desc);

void setup_feature_descs_short();

std::vector<dungeon_feature_type> features_by_desc(const base_pattern &pattern);

void full_describe_view(void);

extern const struct coord_def Compass[8];

#endif
