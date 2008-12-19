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
    coord_def   pos;                   // actual position
    dice_def    damage;
    int         ench_power, hit;
    killer_type thrower;               // what kind of thing threw this?
    char        ex_size;               // explosion radius (0==none)
    int         beam_source;           // NON_MONSTER or monster index #
    std::string name;
    std::string short_name;
    bool        is_beam;               // beams? (can hits multiple targets?)
    bool        is_explosion;
    bool        is_big_cloud;          // expands into big_cloud at endpoint
    bool        aimed_at_spot;         // aimed at (x,y), should not cross
    std::string aux_source;            // source of KILL_MISC beams

    bool        affects_nothing;       // should not hit monsters or features

    bool        effect_known;          // did we _know_ this would happen?

    int         delay;                 // delay used when drawing beam.

    // OUTPUT parameters (tracing, ID)
    bool        obvious_effect;        // did an 'obvious' effect happen?

    int         fr_count, foe_count;   // # of times a friend/foe is "hit"
    int         fr_power, foe_power;   // total levels/hit dice affected
    int         fr_hurt, foe_hurt;     // # of friends/foes actually hurt
    int         fr_helped, foe_helped; // # of friends/foes actually helped

    bool        dropped_item;          // item has been dropped
    coord_def   item_pos;              // position item was dropped at
    int         item_index;            // mitm[index] of item

    bool        seen;                  // Has player seen the beam?

    // INTERNAL use - should not usually be set outside of beam.cc
    int         range_used;
    bool        is_tracer;       // is this a tracer?
    bool        aimed_at_feet;   // this was aimed at self!
    bool        msg_generated;   // an appropriate msg was already mpr'd
    bool        in_explosion_phase;   // explosion phase (as opposed to beam phase)
    bool        smart_monster;   // tracer firer can guess at other mons. resists?
    bool        can_see_invis;   // tracer firer can see invisible?
    mon_attitude_type attitude;  // attitude of whoever fired tracer
    int         foe_ratio;       // 100* foe ratio (see mons_should_fire())
    bool        chose_ray;       // do we want a specific ray?
    bool        beam_cancelled;  // stop_attack_prompt() returned true
    bool        dont_stop_foe;   // stop_attack_prompt() returned false for foe
    bool        dont_stop_fr;    // stop_attack_prompt() returned false for
                                 // friend
    bool        dont_stop_player; // player answered self target prompt with 'y'

    int         bounces;         // # times beam bounced off walls
    coord_def   bounce_pos;      // position of latest wall bounce,
                                 // reset if a reflection happens

    int         reflections;     // # times beam reflected off shields
    int         reflector;       // latest thing to reflect beam

    ray_def     ray;             // shoot on this specific ray


public:
    // A constructor to try and fix some of the bugs that occur because
    // this struct never seems to be properly initialized.  Definition
    // is over in beam.cc.
    bolt();

    bool is_enchantment() const; // no block/dodge, but mag resist
    void set_target(const dist &);
    void setup_retrace();

    // Returns YOU_KILL or MON_KILL, depending on the source of the beam.
    killer_type  killer() const;

    actor* agent() const;

    // Returns member short_name if set, otherwise some reasonble string
    // for a short name, most likely the name of the beam's flavour.
    std::string get_short_name();
};

dice_def calc_dice( int num_dice, int max_damage );

// Test if the to-hit (attack) beats evasion (defence).
bool test_beam_hit(int attack, int defence);
void fire_beam(bolt &pbolt);

int explosion( bolt &pbolt, bool hole_in_the_middle = false,
               bool explode_in_wall = false,
               bool stop_at_statues = true,
               bool stop_at_walls   = true,
               bool show_more       = true,
               bool affect_items    = true);

int mons_adjust_flavoured(monsters *monster, bolt &pbolt, int hurted,
                          bool doFlavouredEffects = true);


// returns true if messages were generated during the enchantment
bool mass_enchantment( enchant_type wh_enchant, int pow, int who,
                       int *m_succumbed = NULL, int *m_attempted = NULL );


mon_resist_type mons_ench_f2(monsters *monster, bolt &pbolt);


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
bool zapping( zap_type ztype, int power, struct bolt &pbolt,
              bool needs_tracer = false, std::string msg = "" );
bool player_tracer( zap_type ztype, int power, struct bolt &pbolt,
                    int range = 0 );
int affect(bolt &beam, const coord_def& p = coord_def(),
           item_def *item = NULL, bool affect_items = true);
void beam_drop_object( bolt &beam, item_def *item = NULL,
                       const coord_def& where = coord_def() );

std::string beam_type_name(beam_type type);

#endif
