/**
 * @file
 * @brief Functions related to ranged attacks.
**/


#ifndef BEAM_H
#define BEAM_H


#include "externs.h"
#include "random.h"
#include "ray.h"
#include "spl-cast.h"

class monster;

enum mon_resist_type
{
    MON_RESIST,                 // monster resisted
    MON_UNAFFECTED,             // monster unaffected
    MON_AFFECTED,               // monster was affected
    MON_OTHER,                  // monster unaffected, but for other reasons
};

class dist;

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

typedef bool (*range_used_func)(const bolt& beam, int &used);
typedef bool (*beam_damage_func)(bolt& beam, actor* victim, int &dmg,
                                 string &dmg_msg);
typedef bool (*beam_hit_func)(bolt& beam, actor* victim, int dmg);
typedef bool (*explosion_aoe_func)(bolt& beam, const coord_def& target);
typedef bool (*beam_affect_func)(const bolt &beam, const actor *victim);

struct bolt
{
    // INPUT parameters set by caller
    spell_type  origin_spell;          // may be SPELL_NO_SPELL for non-spell
                                       // beams.
    int         range;
    unsigned    glyph;                 // missile gfx
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

    // beam_source can be -GOD_ENUM_VALUE besides monster indices
    // and MHITNOT, MHITYOU.
    int         beam_source;           // NON_MONSTER or monster index #
    string      source_name;           // The name of the source, if it
                                       // should be different from
                                       // actor->name(), or the actor dies
                                       // prematurely.
    string      name;
    string      short_name;
    string      hit_verb;              // The verb to use when this beam hits
                                       // something.  If not set, will use
                                       // "engulfs" if an explosion or cloud
                                       // and "hits" otherwise.
    int         loudness;              // Noise level on hitting or exploding.
    string      noise_msg;             // Message to give player if the hit
                                       // or explosion isn't in view.
    bool        is_beam;               // beam? (can hit multiple targets?)
    bool        is_explosion;
    bool        is_big_cloud;          // expands into big_cloud at endpoint
    bool        aimed_at_spot;         // aimed at (x, y), should not cross
    string      aux_source;            // source of KILL_MISC beams

    bool        affects_nothing;       // should not hit monsters or features
    bool        affects_items;         // hits items on ground/inventory

    bool        effect_known;          // did we _know_ this would happen?
    bool        effect_wanton;         // could we have guessed it would happen?

    int         draw_delay;            // delay used when drawing beam.

    bolt*       special_explosion;     // For exploding with a different
                                       // flavour/damage/etc than the beam
                                       // itself.
    bool        was_missile;           // For determining if this was SPMSL_FLAME / FROST etc
                                       // this is required in order to change mulch rate on these types
    bool        animate;               // Do we draw animations?
    ac_type     ac_rule;               // How does defender's AC affect damage.
#ifdef DEBUG_DIAGNOSTICS
    bool        quiet_debug;           // Disable any debug spam.
#endif

    // Various callbacks.
    vector<range_used_func>  range_funcs;
    vector<beam_damage_func> damage_funcs;
    vector<beam_hit_func>    hit_funcs;
    vector<explosion_aoe_func> aoe_funcs; // Function for if the explosion only
                                          // affects certain grid positions.

    // Test if the beam can affect a particular actor.
    beam_affect_func affect_func;

    // OUTPUT parameters (tracing, ID)
    bool        obvious_effect;        // did an 'obvious' effect happen?

    bool        seen;                  // Has player seen the beam?
    bool        heard;                 // Has the player heard the beam?

    vector<coord_def> path_taken;      // Path beam took.

    // INTERNAL use - should not usually be set outside of beam.cc
    int         extra_range_used;
    bool        is_tracer;       // is this a tracer?
    bool        is_targetting;   // . . . in particular, a targetting tracer?
    bool        aimed_at_feet;   // this was aimed at self!
    bool        msg_generated;   // an appropriate msg was already mpr'd
    bool        noise_generated; // a noise has already been generated at this pos
    bool        passed_target;   // Beam progressed beyond target.
    bool     in_explosion_phase; // explosion phase (as opposed to beam phase)
    bool        smart_monster;   // tracer firer can guess at other mons. resists?
    bool        can_see_invis;   // tracer firer can see invisible?
    bool        nightvision;     // tracer firer has nightvision?
    mon_attitude_type attitude;  // attitude of whoever fired tracer
    int         foe_ratio;       // 100* foe ratio (see mons_should_fire())
    map<mid_t, int> hit_count;   // how many times targets were affected

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

    actor* agent(bool ignore_reflections = false) const;

    void fire();

    // Returns member short_name if set, otherwise some reasonable string
    // for a short name, most likely the name of the beam's flavour.
    string get_short_name() const;
    string get_source_name() const;

    // Assume that all saving throws are failed, actually apply
    // the enchantment.
    mon_resist_type apply_enchantment_to_monster(monster* mon);

    // Return whether any affected cell was seen.
    bool explode(bool show_more = true, bool hole_in_the_middle = false);
    bool knockback_actor(actor *actor);

    bool visible() const;

    bool can_affect_actor(const actor *act) const;
    bool ignores_monster(const monster* mon) const;

    maybe_bool affects_wall(dungeon_feature_type wall) const;

private:
    void do_fire();
    coord_def pos() const;
    coord_def leg_source() const;

    // Lots of properties of the beam.
    bool is_blockable() const;
    bool is_fiery() const;
    bool is_superhot() const;
    bool can_affect_wall(dungeon_feature_type feat) const;
    bool can_affect_wall_actor(const actor *act) const;
    bool actor_wall_shielded(const actor *act) const;
    bool is_bouncy(dungeon_feature_type feat) const;
    bool stop_at_target() const;
    bool has_saving_throw() const;
    bool is_harmless(const monster* mon) const;
    bool harmless_to_player() const;
    bool is_reflectable(const item_def *item) const;
    bool nasty_to(const monster* mon) const;
    bool nice_to(const monster* mon) const;
    bool found_player() const;
    bool need_regress() const;

    const actor* beam_source_as_target() const;

    int range_used_on_hit() const;

    string zapper() const;

    set<string> message_cache;
    void emit_message(const char* msg);
    void step();
    bool hit_wall();

    bool apply_hit_funcs(actor* victim, int dmg);
    bool apply_dmg_funcs(actor* victim, int &dmg, vector<string> &messages);
    int apply_AC(const actor* victim, int hurted);

    cloud_type get_cloud_type();
    int get_cloud_pow();
    int get_cloud_size(bool min = false, bool max = false);

    // Functions which handle actually affecting things. They all
    // operate on the beam's current position (i.e., whatever pos()
    // returns.)
public:
    void affect_cell();
    void affect_wall();
    void affect_actor(actor *act);
    void affect_monster(monster* m);
    void affect_player();
    void affect_ground();
    void affect_place_clouds();
    void affect_place_explosion_clouds();
    void affect_endpoint();

    void beam_hits_actor(actor *act);
    bool god_cares() const; // Will the god be unforgiving about this beam?

    // Stuff when a monster or player is hit.
    void affect_player_enchantment();
    void tracer_affect_player();
    void tracer_affect_monster(monster* mon);
    bool handle_statue_disintegration(monster* mon);
    void apply_bolt_paralysis(monster* mons);
    void apply_bolt_petrify(monster* mons);
    void enchantment_affect_monster(monster* mon);
    mon_resist_type try_enchant_monster(monster* mon, int &res_margin);
    void tracer_enchantment_affect_monster(monster* mon);
    void tracer_nonenchantment_affect_monster(monster* mon);
    void update_hurt_or_helped(monster* mon);
    bool attempt_block(monster* mon);
    void handle_stop_attack_prompt(monster* mon);
    bool determine_damage(monster* mon, int& preac, int& postac, int& final,
                          vector<string> &messages);
    void monster_post_hit(monster* mon, int dmg);
    bool misses_player();

    void initialise_fire();
    void apply_beam_conducts();
    void choose_ray();
    void draw(const coord_def& p);
    void bounce();
    void reflect();
    void fake_flavour();
    void digging_wall_effect();
    void fire_wall_effect();
    void elec_wall_effect();
    void nuke_wall_effect();
    void drop_object();
    int range_used(bool leg_only = false) const;
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

int mons_adjust_flavoured(monster* mons, bolt &pbolt, int hurted,
                          bool doFlavouredEffects = true);

// Return whether the effect was visible.
bool enchant_monster_with_flavour(monster* mon, actor *atk,
                                  beam_type flavour, int powc = 0);

bool enchant_monster_invisible(monster* mon, const string &how);

spret_type mass_enchantment(enchant_type wh_enchant, int pow,
                            bool fail = false);

bool poison_monster(monster* mons, const actor* who, int levels = 1,
                    bool force = false, bool verbose = true);
bool miasma_monster(monster* mons, const actor* who);
bool napalm_monster(monster* mons, const actor* who, int levels = 1,
                    bool verbose = true);
void fire_tracer(const monster* mons, bolt &pbolt,
                  bool explode_only = false);
bool imb_can_splash(coord_def origin, coord_def center,
                    vector<coord_def> path_taken, coord_def target);
spret_type zapping(zap_type ztype, int power, bolt &pbolt,
                   bool needs_tracer = false, const char* msg = NULL,
                   bool fail = false);
bool player_tracer(zap_type ztype, int power, bolt &pbolt, int range = 0);

void init_zap_index();
void clear_zap_info_on_exit();

int zap_power_cap(zap_type ztype);
void zappy(zap_type z_type, int power, bolt &pbolt);

#endif
