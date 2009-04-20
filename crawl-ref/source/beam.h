/*
 *  File:       beam.h
 *  Summary:    Functions related to ranged attacks.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef BEAM_H
#define BEAM_H


#include "externs.h"
#include "ray.h"

class monsters;

enum demon_beam_type
{
    DMNBM_HELLFIRE,
    DMNBM_SMITING,
    DMNBM_BRAIN_FEED,
    DMNBM_MUTATION
};

enum mon_resist_type
{
    MON_RESIST,                 // monster resisted
    MON_UNAFFECTED,             // monster unaffected
    MON_AFFECTED,               // monster was affected
    MON_OTHER                   // monster unaffected, but for other reasons
};

struct dist;

typedef FixedArray<int, 19, 19> explosion_map;

struct tracer_info
{
    int count;                         // # of times something "hit"
    int power;                         // total levels/hit dice affected
    int hurt;                          // # of things actually hurt
    int helped;                        // # of things actually helped
    bool dont_stop;                    // Player said not to stop on this

    tracer_info();
    void reset();

    const tracer_info &operator += (const tracer_info &other);
};

struct bolt;

typedef bool (*range_used_func)(const bolt& beam, const actor* victim,
                                int &used);
typedef bool (*beam_damage_func)(bolt& beam, actor* victim, int &dmg,
                                 std::string &dmg_msg);
typedef bool (*beam_hit_func)(bolt& beam, actor* victim, int dmg,
                              int corpse);

struct bolt
{
    // INPUT parameters set by caller
    int         range;
    unsigned    type;                  // missile gfx
    int         colour;
    beam_type   flavour;
    beam_type   real_flavour;          // for random and chaos beams this
                                       // will remain the same while flavour
                                       // changes
    bool        drop_item;             // should drop an item when done
    item_def*   item;                  // item to drop
    coord_def   source;                // beam origin
    coord_def   target;                // intended target
    dice_def    damage;
    int         ench_power, hit;
    killer_type thrower;               // what kind of thing threw this?
    int         ex_size;               // explosion radius (0==none)
    int         beam_source;           // NON_MONSTER or monster index #
    std::string name;
    std::string short_name;
    bool        is_beam;               // beams? (can hits multiple targets?)
    bool        is_explosion;
    bool        is_big_cloud;          // expands into big_cloud at endpoint
    bool        aimed_at_spot;         // aimed at (x,y), should not cross
    std::string aux_source;            // source of KILL_MISC beams

    bool        affects_nothing;       // should not hit monsters or features
    bool        affects_items;         // hits items on ground/inventory

    bool        effect_known;          // did we _know_ this would happen?

    int         draw_delay;            // delay used when drawing beam.

    bolt*       special_explosion;     // For exploding with a different
                                       // flavour/damage/etc than the beam
                                       // itself.

    // Various callbacks.
    std::vector<range_used_func>  range_funcs;
    std::vector<beam_damage_func> damage_funcs;
    std::vector<beam_hit_func>    hit_funcs;

    // OUTPUT parameters (tracing, ID)
    bool        obvious_effect;        // did an 'obvious' effect happen?

    bool        seen;                  // Has player seen the beam?

    std::vector<coord_def> path_taken; // Path beam took.

    // INTERNAL use - should not usually be set outside of beam.cc
    int         range_used;
    bool        is_tracer;       // is this a tracer?
    bool        aimed_at_feet;   // this was aimed at self!
    bool        msg_generated;   // an appropriate msg was already mpr'd
    bool     in_explosion_phase; // explosion phase (as opposed to beam phase)
    bool        smart_monster;   // tracer firer can guess at other mons. resists?
    bool        can_see_invis;   // tracer firer can see invisible?
    mon_attitude_type attitude;  // attitude of whoever fired tracer
    int         foe_ratio;       // 100* foe ratio (see mons_should_fire())

    tracer_info foe_info;
    tracer_info friend_info;

    bool        chose_ray;       // do we want a specific ray?
    bool        beam_cancelled;  // stop_attack_prompt() returned true
    bool        dont_stop_player; // player answered self target prompt with 'y'

    int         bounces;         // # times beam bounced off walls
    coord_def   bounce_pos;      // position of latest wall bounce,
                                 // reset if a reflection happens

    int         reflections;     // # times beam reflected off shields
    int         reflector;       // latest thing to reflect beam

    bool        use_target_as_pos; // pos() should return ::target()
    bool        auto_hit;

    ray_def     ray;             // shoot on this specific ray

#ifdef USE_TILE
    int         tile_beam;
#endif

public:
    bolt();

    bool is_enchantment() const; // no block/dodge, use magic resist
    void set_target(const dist &targ);
    void set_agent(actor *agent);
    void setup_retrace();

    // Returns YOU_KILL or MON_KILL, depending on the source of the beam.
    killer_type  killer() const;

    kill_category whose_kill() const;

    actor* agent() const;

    void fire();

    // Returns member short_name if set, otherwise some reasonable string
    // for a short name, most likely the name of the beam's flavour.
    std::string get_short_name() const;

    // Assume that all saving throws are failed, actually apply
    // the enchantment.
    mon_resist_type apply_enchantment_to_monster(monsters* mon);

    // Return whether any affected cell was seen.
    bool explode(bool show_more = true, bool hole_in_the_middle = false);

private:
    void do_fire();
    coord_def pos() const;

    // Lots of properties of the beam.
    bool is_blockable() const;
    bool is_superhot() const;
    bool is_fiery() const;
    bool affects_wall(dungeon_feature_type wall) const;
    bool is_bouncy(dungeon_feature_type feat) const;
    bool can_affect_wall_monster(const monsters* mon) const;
    bool stop_at_target() const;
    bool invisible() const;
    bool has_saving_throw() const;
    bool is_harmless(const monsters *mon) const;
    bool harmless_to_player() const;
    bool is_reflectable(const item_def *item) const;
    bool nasty_to(const monsters* mon) const;
    bool nice_to(const monsters* mon) const;
    bool found_player() const;

    int beam_source_as_target() const;
    int range_used_on_hit(const actor* victim) const;

    std::string zapper() const;

    std::set<std::string> message_cache;
    void emit_message(msg_channel_type chan, const char* msg);
    void step();
    void hit_wall();

    bool apply_hit_funcs(actor* victim, int dmg, int corpse = -1);
    bool apply_dmg_funcs(actor* victim, int &dmg,
                         std::vector<std::string> &messages);

    // Functions which handle actually affecting things. They all
    // operate on the beam's current position (i.e., whatever pos()
    // returns.)
public:
    void affect_cell();
    void affect_wall();
    void affect_monster( monsters* m );
    void affect_player();
    void affect_ground();
    void affect_place_clouds();
    void affect_place_explosion_clouds();
    void affect_endpoint();

    // Stuff when a monster or player is hit.
    void affect_player_enchantment();
    void tracer_affect_player();
    void tracer_affect_monster(monsters* mon);
    bool handle_statue_disintegration(monsters* mon);
    void apply_bolt_paralysis(monsters *monster);
    void apply_bolt_petrify(monsters *monster);
    void enchantment_affect_monster(monsters* mon);
    mon_resist_type try_enchant_monster(monsters *mon);
    void tracer_enchantment_affect_monster(monsters* mon);
    void tracer_nonenchantment_affect_monster(monsters* mon);
    void update_hurt_or_helped(monsters *mon);
    bool attempt_block(monsters* mon);
    void handle_stop_attack_prompt(monsters* mon);
    bool determine_damage(monsters* mon, int& preac, int& postac, int& final,
                          std::vector<std::string> &messages);
    void monster_post_hit(monsters* mon, int dmg);
    bool misses_player();

    void initialize_fire();
    void apply_beam_conducts();
    void choose_ray();
    void draw(const coord_def& p);
    void bounce();
    void reflect();
    void fake_flavour();
    void digging_wall_effect();
    void fire_wall_effect();
    void nuke_wall_effect();
    void drop_object();
    void finish_beam();
    bool fuzz_invis_tracer();

    void internal_ouch(int dam);

    // Various explosion-related stuff.
    void refine_for_explosion();
    void explosion_draw_cell(const coord_def& p);
    void explosion_affect_cell(const coord_def& p);
    void determine_affected_cells(explosion_map& m, const coord_def& delta,
                                  int count, int r,
                                  bool stop_at_statues, bool stop_at_walls);
};

dice_def calc_dice( int num_dice, int max_damage );

// Test if the to-hit (attack) beats evasion (defence).
bool test_beam_hit(int attack, int defence);

int mons_adjust_flavoured(monsters *monster, bolt &pbolt, int hurted,
                          bool doFlavouredEffects = true);

// Return whether the effect was visible.
bool enchant_monster_with_flavour(monsters* mon, actor *atk,
                                  beam_type flavour, int powc = 0);

// Return true if messages were generated during the enchantment.
bool mass_enchantment( enchant_type wh_enchant, int pow, int who,
                       int *m_succumbed = NULL, int *m_attempted = NULL );

bool curare_hits_monster(actor *agent, monsters *monster, kill_category who,
                         int levels = 1);
bool poison_monster(monsters *monster, kill_category who, int levels = 1,
                    bool force = false, bool verbose = true);
bool napalm_monster(monsters *monster, kill_category who, int levels = 1,
                    bool verbose = true);
void fire_tracer( const monsters *monster, struct bolt &pbolt,
                  bool explode_only = false );
bool check_line_of_sight( const coord_def& source, const coord_def& target );
void mimic_alert( monsters *mimic );
bool zapping(zap_type ztype, int power, bolt &pbolt,
             bool needs_tracer = false, const char* msg = NULL);
bool player_tracer(zap_type ztype, int power, bolt &pbolt, int range = 0);

std::string beam_type_name(beam_type type);

#endif
