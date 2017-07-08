/**
 * @file
 * @brief Functions used when picking squares.
**/

#pragma once

#include "command-type.h"
#include "enum.h"
#include "mon-info.h"
#include "targ-mode-type.h"
#include "targeting-type.h"
#include "trap-type.h"

struct describe_info;

class range_view_annotator
{
public:
    range_view_annotator(targeter *range);
    virtual ~range_view_annotator();
};

class monster_view_annotator
{
public:
    monster_view_annotator(vector<monster *> *monsters);
    virtual ~monster_view_annotator();
};

// An object that modifies the behaviour of the direction prompt.
class targeting_behaviour
{
public:
    targeting_behaviour(bool just_looking = false);
    virtual ~targeting_behaviour();

    // Returns a keystroke for the prompt.
    virtual int get_key();
    virtual command_type get_command(int key = -1);

    // Should we force a redraw?
    virtual bool should_redraw() const { return false; }
    // Clear the bit set above.
    virtual void clear_redraw()  { return; }

    // Update the prompt shown at top.
    virtual void update_top_prompt(string* p_top_prompt) {}

    // Add relevant descriptions to the target status.
    virtual vector<string> get_monster_desc(const monster_info& mi);
private:
    string prompt;

public:
    bool just_looking;
    bool compass;
    desc_filter get_desc_func; // Function to add relevant descriptions
};

// output from direction() function:
class dist
{
public:
    dist();

    bool isMe() const;

    // modify target as if the player is confused.
    void confusion_fuzz(int range = 6);

    bool isValid;       // valid target chosen?
    bool isTarget;      // target (true), or direction (false)?
    bool isEndpoint;    // Does the player want the attack to stop at target?
    bool isCancel;      // user cancelled (usually <ESC> key)
    bool choseRay;      // user wants a specific beam

    coord_def target;   // target x,y or logical extension of beam to map edge
    coord_def delta;    // delta x and y if direction - always -1,0,1
    ray_def ray;        // ray chosen if necessary
};

struct direction_chooser_args
{
    targeter *hitfunc;
    targeting_type restricts;
    targ_mode_type mode;
    int range;
    bool just_looking;
    bool needs_path;
    confirm_prompt_type self;
    const char *target_prefix;
    string top_prompt;
    targeting_behaviour *behaviour;
    bool show_floor_desc;
    desc_filter get_desc_func;
    coord_def default_place;

    direction_chooser_args() :
        hitfunc(nullptr),
        restricts(DIR_NONE),
        mode(TARG_ANY),
        range(-1),
        just_looking(false),
        needs_path(true),
        self(CONFIRM_PROMPT),
        target_prefix(nullptr),
        behaviour(nullptr),
        show_floor_desc(false),
        get_desc_func(nullptr),
        default_place(0, 0) {}
};

class direction_chooser
{
public:
    direction_chooser(dist& moves, const direction_chooser_args& args);
    bool choose_direction();

private:
    bool targets_objects() const;
    bool targets_enemies() const;
    bool choose_compass();      // Used when we only need to choose a direction

    bool do_main_loop();

    // Return the location where targeting should start.
    coord_def find_default_target() const;

    void handle_mlist_cycle_command(command_type key_command);
    void handle_wizard_command(command_type key_command, bool* loop_done);
    void handle_movement_key(command_type key_command, bool* loop_done);

    bool in_range(const coord_def& p) const;

    // Jump to the player.
    void move_to_you();

    // Cycle to the next (dir == 1) or previous (dir == -1) object.
    void object_cycle(int dir);

    // Cycle to the next (dir == 1) or previous (dir == -1) monster.
    void monster_cycle(int dir);

    // Cycle to the next feature of the given type.
    void feature_cycle_forward(int feature);

    // Set the remembered target to be the current target.
    void update_previous_target() const;

    // Finalise the current choice of target. Return true if
    // successful, false if failed (e.g. out of range.)
    bool select(bool allow_out_of_range, bool endpoint);
    bool select_compass_direction(const coord_def& delta);
    bool select_previous_target();

    // Mark item for pickup, initiate movement.
    bool pickup_item();

    // Return true if we need to abort targeting due to a signal.
    bool handle_signals();

    void reinitialize_move_flags();

    // Return or set the current target.
    const coord_def& target() const;
    void set_target(const coord_def& new_target);

    string build_targeting_hint_string() const;

    actor* targeted_actor() const;
    monster* targeted_monster() const;

    bool find_default_monster_target(coord_def& result) const;
    // Functions which print things to the user.
    // Each one is commented with a sample output.

    // Whatever the caller defines. Typically something like:
    // Casting: Venom Bolt.
    // Can be modified by the targeting_behaviour.
    void print_top_prompt() const;

    // Press: ? - help, Shift-Dir - straight line, t - megabat
    void print_key_hints() const;

    // Here: An orc wizard, wielding a glowing orcish dagger, and wearing
    // an orcish robe (miasma, silenced, almost dead)
    // OR:
    // Apport: A short sword.
    void print_target_description(bool &did_cloud) const;

    // Helper functions for the above.
    void print_target_monster_description(bool &did_cloud) const;
    void print_target_object_description() const;

    // You see 2 +3 dwarven bolts here.
    // There is something else lying underneath.
    void print_items_description() const;

    // Lava.
    //
    // If boring_too is false, then don't print anything on boring
    // terrain (i.e. floor.)
    void print_floor_description(bool boring_too) const;

    string target_interesting_terrain_description() const;
    string target_cloud_description() const;
    string target_sanctuary_description() const;
    string target_silence_description() const;
    vector<string> target_cell_description_suffixes() const;
    vector<string> monster_description_suffixes(const monster_info& mi) const;

    void describe_cell() const;

    // Move the target to where the mouse pointer is (on tiles.)
    // Returns whether the move was valid, i.e., whether the mouse
    // pointer is in bounds.
    bool tiles_update_target();

    // Display the prompt when beginning targeting.
    void show_initial_prompt();

    void toggle_beam();

    void finalize_moves();
    void draw_beam_if_needed();
    void do_redraws();

    // Whether the current target is you.
    bool looking_at_you() const;

    // Whether the current target is valid.
    bool move_is_ok() const;

    void describe_target();
    void show_help();

    // Parameters.
    dist& moves;                // Output.
    targeting_type restricts;   // What kind of target do we want?
    targ_mode_type mode;        // Hostiles or friendlies?
    int range;                  // Max range to consider
    bool just_looking;
    bool needs_path;            // Determine a ray while we're at it?
    confirm_prompt_type self;   // What do when aiming at yourself
    const char *target_prefix;  // A string displayed before describing target
    string top_prompt;          // Shown at the top of the message window
    targeting_behaviour *behaviour; // Can be nullptr for default
    bool show_floor_desc;       // Describe the floor of the current target
    targeter *hitfunc;         // Determine what would be hit.
    coord_def default_place;    // Start somewhere other than you.pos()?

    // Internal data.
    ray_def beam;               // The (possibly invalid) beam.
    bool show_beam;             // Does the user want the beam displayed?
    bool have_beam;             // Is the currently stored beam valid?
    coord_def objfind_pos, monsfind_pos; // Cycling memory

    bool valid_shadow_step;     // If shadow-stepping, do we currently have a
                                // monster target with a valid landing
                                // position?

    // What we need to redraw.
    bool need_beam_redraw;
    bool need_cursor_redraw;
    bool need_text_redraw;
    bool need_all_redraw;       // All of the above.

    bool show_items_once;       // Should we show items this time?

    // Default behaviour, saved across instances.
    static targeting_behaviour stock_behaviour;
};

// Monster equipment description level.
enum mons_equip_desc_level_type
{
    DESC_WEAPON,
    DESC_FULL,
    DESC_IDENTIFIED,
    DESC_WEAPON_WARNING, // like DESC_WEAPON but also includes dancing weapons
};

void direction(dist &moves, const direction_chooser_args& args);

string get_terse_square_desc(const coord_def &gc);
void terse_describe_square(const coord_def &c, bool in_range = true);
void full_describe_square(const coord_def &c, bool cleanup = true);
void get_square_desc(const coord_def &c, describe_info &inf);

void describe_floor();
string get_monster_equipment_desc(const monster_info& mi,
                                  //bool full_desc = true,
                                  mons_equip_desc_level_type level = DESC_FULL,
                                  description_level_type mondtype = DESC_A,
                                  bool print_attitude = false);

string feature_description_at(const coord_def& where, bool covering = false,
                              description_level_type dtype = DESC_A,
                              bool add_stop = true);
string raw_feature_description(const coord_def& where);
string feature_description(dungeon_feature_type grid,
                           trap_type trap = NUM_TRAPS,
                           const string & cover_desc = "",
                           description_level_type dtype = DESC_A,
                           bool add_stop = true);

vector<dungeon_feature_type> features_by_desc(const base_pattern &pattern);

void full_describe_view();
void do_look_around(const coord_def &whence = coord_def(0, 0));

extern const struct coord_def Compass[9];
