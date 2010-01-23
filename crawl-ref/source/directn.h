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

    bool isMe() const;

    bool isValid;       // valid target chosen?
    bool isTarget;      // target (true), or direction (false)?
    bool isEndpoint;    // Does the player want the attack to stop at target?
    bool isCancel;      // user cancelled (usually <ESC> key)
    bool choseRay;      // user wants a specific beam
    coord_def target;   // target x,y or logical extension of beam to map edge
    coord_def delta;    // delta x and y if direction - always -1,0,1
    ray_def ray;        // ray chosen if necessary
};

class direction_chooser
{
public:
    // FIXME: wrap all these parameters in a struct.
    direction_chooser(dist& moves_,
                      targetting_type restricts_ = DIR_NONE,
                      targ_mode_type mode_ = TARG_ANY,
                      int range_ = -1,
                      bool just_looking_ = false,
                      bool needs_path_ = true,
                      bool may_target_monster_ = true,
                      bool may_target_self_ = false,
                      const char *prompt_prefix_ = NULL,
                      targetting_behaviour *mod_ = NULL,
                      bool cancel_at_self_ = false);
    bool choose_direction();

private:
    bool choose_again();
    bool choose_compass();
    bool do_main_loop();
    coord_def find_default_target() const;
    void draw_beam_if_needed();
    void handle_mlist_cycle_command(command_type key_command);
    void handle_wizard_command(command_type key_command, bool* loop_done);
    void handle_movement_key(command_type key_command, bool* loop_done);
    bool in_range(const coord_def& p) const;
    void move_to_you();
    void object_cycle(int dir);
    void monster_cycle(int dir);
    void feature_cycle_forward(int feature);
    void update_previous_target() const;
    // Finalise the current choice of target. Return true if
    // successful, false if failed (e.g. out of range.)
    bool select(bool allow_out_of_range, bool endpoint);
    bool handle_signals();
    void reinitialize_move_flags();
    bool select_compass_direction(const coord_def& delta);

    const coord_def& target() const;
    void set_target(const coord_def& new_target);

    // Functions which print things to the user.
    // Each one is commented with a sample output.

    // Aim (? - help, Shift-Dir - straight line, p/t - orc wizard)
    void print_aim_prompt() const;

    // An almost dead orc wizard, wielding a glowing orcish dagger,
    // and wearing an orcish robe.
    void print_target_description() const;

    // You see 2 +3 dwarven bolts here.
    // There is something else lying underneath.
    void print_items_description() const;

    // Lava.
    //
    // If boring_too is false, then don't print anything on boring
    // terrain (i.e. floor.)
    void print_floor_description(bool boring_too) const;

    // Move the target to where the mouse pointer is (on tiles.)
    // Returns whether the move was valid, i.e., whether the mouse
    // pointer is in bounds.
    bool tiles_update_target();
    
    void show_initial_prompt() const;
    bool select_previous_target();
    void toggle_beam();
    void finalize_moves();
    command_type massage_command(command_type key_command) const;
    void do_redraws();
    bool looking_at_you() const;
    bool move_is_ok() const;
    void describe_target();
    void show_help();

    // Parameters.
    dist& moves;                // Output.
    targetting_type restricts;  // What kind of target do we want?
    targ_mode_type mode;        // Hostiles or friendlies?
    int range;                  // Max range to consider
    bool just_looking;
    bool needs_path;            // Determine a ray while we're at it?
    bool may_target_monster;
    bool may_target_self;       // ?? XXX Used only for _init_mlist() currently
    const char *prompt_prefix;  // Initial prompt to show (or NULL)
    targetting_behaviour *behaviour; // Can be NULL for default
    bool cancel_at_self;        // Disallow self-targetting?

    // Internal data.
    ray_def beam;               // The (possibly invalid) beam.
    bool show_beam;             // Does the user want the beam displayed?
    bool have_beam;             // Is the currently stored beam valid?
    coord_def objfind_pos, monsfind_pos; // Cycling memory
    bool need_beam_redraw;
    bool need_cursor_redraw;
    bool need_text_redraw;
    bool need_all_redraw;
    bool target_unshifted;      // Do unshifted direction keys fire?

    // Default behaviour, saved across instances.
    static targetting_behaviour stock_behaviour;
    
};

void direction( dist &moves, targetting_type restricts = DIR_NONE,
                targ_mode_type mode = TARG_ANY, int range = -1,
                bool just_looking = false, bool needs_path = true,
                bool may_target_monster = true, bool may_target_self = false,
                const char *prompt_prefix = NULL,
                targetting_behaviour *mod = NULL, bool cancel_at_self = false );

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
