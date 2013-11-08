#ifndef ACTOR_H
#define ACTOR_H

#include "itemprop-enum.h"

enum ev_ignore_type
{
    EV_IGNORE_NONE       = 0,
    EV_IGNORE_HELPLESS   = 1,
    EV_IGNORE_PHASESHIFT = 2,
};

struct bolt;

/* Axe this block soon */
// (needed for asserts in is_player())
class player;
extern player you;
/***********************/

class actor
{
public:
    virtual ~actor();

    monster_type type;
    mid_t        mid;
    virtual int       mindex() const = 0;
    virtual actor_type atype() const = 0;

    bool is_player() const
    {
        ASSERT(this);
        if (atype() == ACT_PLAYER)
        {
#ifndef DEBUG_GLOBALS
            ASSERT(this == (actor*)&you); // there can be only one
#endif
            return true;
        }
        return false;
    }
    bool is_monster() const { return !is_player(); }
    virtual monster* as_monster() = 0;
    virtual player* as_player() = 0;
    virtual const monster* as_monster() const = 0;
    virtual const player* as_player() const = 0;

    virtual kill_category kill_alignment() const = 0;
    virtual god_type  deity() const = 0;

    virtual bool      alive() const = 0;

    // Should return false for perma-summoned things.
    virtual bool is_summoned(int* duration = NULL,
                             int* summon_type = NULL) const = 0;

    virtual bool is_perm_summoned() const = 0;

    // [ds] Low-level moveto() - moves the actor without updating relevant
    // grids, such as mgrd.
    virtual void moveto(const coord_def &c, bool clear_net = true) = 0;

    // High-level actor movement. If in doubt, use this. Returns false if the
    // actor cannot be moved to the target, possibly because it is already
    // occupied.
    virtual bool move_to_pos(const coord_def &c, bool clear_net = true) = 0;

    virtual void apply_location_effects(const coord_def &oldpos,
                                        killer_type killer = KILL_NONE,
                                        int killernum = -1) = 0;

    virtual void set_position(const coord_def &c);
    virtual const coord_def& pos() const { return position; }

    virtual bool self_destructs() { return false; }

    // Blink the actor to the destination. c should be a
    // valid target, though the method returns false
    // if the blink fails.
    virtual bool blink_to(const coord_def &c, bool quiet = false) = 0;

    virtual bool      swimming() const = 0;
    virtual bool      submerged() const = 0;
    virtual bool      floundering() const = 0;

    // Returns true if the actor is exceptionally well balanced.
    virtual bool      extra_balanced() const = 0;

    virtual int       get_experience_level() const = 0;

    virtual bool shove(const char* feat_name = "") = 0;
    virtual bool can_pass_through_feat(dungeon_feature_type grid) const = 0;
    virtual bool can_pass_through(int x, int y) const;
    virtual bool can_pass_through(const coord_def &c) const;

    virtual bool is_habitable_feat(dungeon_feature_type actual_grid) const = 0;
            bool is_habitable(const coord_def &pos) const;

    virtual size_type body_size(size_part_type psize = PSIZE_TORSO,
                                bool base = false) const = 0;
    virtual int       body_weight(bool base = false) const;
    virtual int       total_weight() const = 0;

    virtual brand_type damage_brand(int which_attack = -1) = 0;
    virtual int       damage_type(int which_attack = -1) = 0;
    virtual item_def *weapon(int which_attack = -1) const = 0;
    const item_def *primary_weapon() const
    {
        return weapon(0);
    }
    virtual int has_claws(bool allow_tran = true) const = 0;
    virtual item_def *shield() const = 0;
    virtual item_def *slot_item(equipment_type eq,
                                bool include_melded=false) const = 0;
    virtual int wearing(equipment_type slot, int sub_type,
                        bool calc_unid = true) const = 0;
    virtual int wearing_ego(equipment_type slot, int sub_type,
                            bool calc_unid = true) const = 0;
    virtual int scan_artefacts(artefact_prop_type which_property,
                               bool calc_unid = true) const = 0;

    virtual hands_reqd_type hands_reqd(const item_def &item) const;

            bool can_wield(const item_def* item,
                           bool ignore_curse = false,
                           bool ignore_brand = false,
                           bool ignore_shield = false,
                           bool ignore_transform = false) const;
    virtual bool can_wield(const item_def &item,
                           bool ignore_curse = false,
                           bool ignore_brand = false,
                           bool ignore_shield = false,
                           bool ignore_transform = false) const = 0;
    virtual bool could_wield(const item_def &item,
                             bool ignore_brand = false,
                             bool ignore_transform = false) const = 0;

    virtual void make_hungry(int nutrition, bool silent = true)
    {
    }

    virtual void lose_energy(energy_use_type, int div = 1, int mult = 1)
    {
    }
    virtual void gain_energy(energy_use_type, int div = 1, int mult = 1)
    {
    }

    virtual string name(description_level_type type,
                        bool force_visible = false) const = 0;
    virtual string pronoun(pronoun_type which_pronoun,
                           bool force_visible = false) const = 0;
    virtual string conj_verb(const string &verb) const = 0;
    virtual string hand_name(bool plural, bool *can_plural = NULL) const = 0;
    virtual string foot_name(bool plural, bool *can_plural = NULL) const = 0;
    virtual string arm_name(bool plural, bool *can_plural = NULL) const = 0;

    virtual bool fumbles_attack(bool verbose = true) = 0;

    virtual bool fights_well_unarmed(int heavy_armour_penalty)
    {
        return true;
    }
    // Returns true if the actor has no way to attack (plants, statues).
    // (statues have only indirect attacks).
    virtual bool cannot_fight() const = 0;
    virtual void attacking(actor *other) = 0;
    virtual bool can_go_berserk() const = 0;
    virtual void go_berserk(bool intentional, bool potion = false) = 0;
    virtual bool berserk() const = 0;
    virtual bool can_see_invisible() const = 0;
    virtual bool invisible() const = 0;
    virtual bool nightvision() const = 0;
    virtual reach_type reach_range() const = 0;
    virtual bool can_jump() const = 0;

    // Would looker be able to see the actor when in LOS?
    virtual bool visible_to(const actor *looker) const = 0;

    // Is the given cell within LOS of the actor?
    virtual bool see_cell(const coord_def &c) const;
    virtual bool see_cell_no_trans(const coord_def &c) const;

    // Can the actor actually see the target?
    virtual bool can_see(const actor *target) const;

    // Visibility as required by messaging. In usual play:
    //   Does the player know what's happening to the actor?
    virtual bool observable() const;

    virtual bool is_icy() const = 0;
    virtual bool is_fiery() const = 0;
    virtual bool is_skeletal() const = 0;
    virtual bool has_lifeforce() const = 0;
    virtual bool can_mutate() const = 0;
    virtual bool can_safely_mutate() const = 0;
    virtual bool can_polymorph() const = 0;
    virtual bool can_bleed(bool allow_tran = true) const = 0;
    virtual bool is_stationary() const = 0;
    virtual bool malmutate(const string &reason) = 0;
    virtual bool polymorph(int pow) = 0;
    virtual bool drain_exp(actor *agent, bool quiet = false, int pow = 15) = 0;
    virtual bool rot(actor *agent, int amount, int immediate = 0,
                     bool quiet = false) = 0;
    virtual int  hurt(const actor *attacker, int amount,
                      beam_type flavour = BEAM_MISSILE,
                      bool cleanup_dead = true,
                      bool attacker_effects = true) = 0;
    virtual bool heal(int amount, bool max_too = false) = 0;
    virtual void banish(actor *agent, const string &who = "") = 0;
    virtual void blink(bool allow_partial_control = true) = 0;
    virtual void teleport(bool right_now = false,
                          bool abyss_shift = false,
                          bool wizard_tele = false) = 0;
    virtual bool poison(actor *attacker, int amount = 1, bool force = false) = 0;
    virtual bool sicken(int amount, bool allow_hint = true, bool quiet = false) = 0;
    virtual void paralyse(actor *attacker, int strength, string source = "") = 0;
    virtual void petrify(actor *attacker, bool force = false) = 0;
    virtual bool fully_petrify(actor *foe, bool quiet = false) = 0;
    virtual void slow_down(actor *attacker, int strength) = 0;
    virtual void confuse(actor *attacker, int strength) = 0;
    virtual void put_to_sleep(actor *attacker, int strength) = 0;
    virtual void weaken(actor *attacker, int pow) = 0;
    virtual void expose_to_element(beam_type element, int strength = 0,
                                   bool damage_inventory = true,
                                   bool slow_cold_blood = true) = 0;
    virtual void drain_stat(stat_type stat, int amount, actor* attacker) { }
    virtual bool can_hibernate(bool holi_only = false,
                               bool intrinsic_only = false) const;
    virtual bool can_sleep(bool holi_only = false) const;
    virtual void hibernate(int power = 0) = 0;
    virtual void check_awaken(int disturbance) = 0;
    virtual int beam_resists(bolt &beam, int hurted, bool doEffects,
                             string source = "") = 0;

    virtual int  skill(skill_type sk, int scale = 1, bool real = false) const = 0;
    int  skill_rdiv(skill_type sk, int mult = 1, int div = 1) const;

    virtual int stat_hp() const = 0;
    virtual int stat_maxhp() const = 0;

    virtual int stealth () const = 0;

    virtual bool can_throw_large_rocks() const = 0;

    virtual int armour_class() const = 0;
    virtual int gdr_perc() const = 0;
    int apply_ac(int damage, int max_damage = 0, ac_type ac_rule = AC_NORMAL,
                 int stab_bypass = 0) const;
    virtual int melee_evasion(const actor *attacker,
                              ev_ignore_type ign = EV_IGNORE_NONE) const = 0;
    virtual int shield_bonus() const = 0;
    virtual int shield_block_penalty() const = 0;
    virtual int shield_bypass_ability(int tohit) const = 0;
    virtual void shield_block_succeeded(actor *foe);
    virtual int missile_deflection() const = 0; // 1 = RMsl, 2 = DMsl

    // Combat-related virtual class methods
    virtual int unadjusted_body_armour_penalty() const = 0;
    virtual int adjusted_body_armour_penalty(int scale = 1,
                                             bool use_size = false) const = 0;
    virtual int adjusted_shield_penalty(int scale) const = 0;
    virtual int armour_tohit_penalty(bool random_factor, int scale = 1) const = 0;
    virtual int shield_tohit_penalty(bool random_factor, int scale = 1) const = 0;

    virtual monster_type mons_species(bool zombie_base = false) const = 0;

    virtual mon_holy_type holiness() const = 0;
    virtual bool undead_or_demonic() const = 0;
    virtual bool is_holy(bool spells = true) const = 0;
    virtual bool is_unholy(bool spells = true) const = 0;
    virtual bool is_evil(bool spells = true) const = 0;
    virtual bool is_chaotic() const = 0;
    virtual bool is_artificial() const = 0;
    virtual bool is_unbreathing() const = 0;
    virtual bool is_insubstantial() const = 0;
    virtual int res_acid(bool calc_unid = true) const = 0;
    virtual int res_fire() const = 0;
    virtual int res_holy_fire() const;
    virtual int res_steam() const = 0;
    virtual int res_cold() const = 0;
    virtual int res_elec() const = 0;
    virtual int res_poison(bool temp = true) const = 0;
    virtual int res_rotting(bool temp = true) const = 0;
    virtual int res_asphyx() const = 0;
    virtual int res_water_drowning() const = 0;
    virtual int res_sticky_flame() const = 0;
    virtual int res_holy_energy(const actor *attacker) const = 0;
    virtual int res_negative_energy(bool intrinsic_only = false) const = 0;
    virtual int res_torment() const = 0;
    virtual int res_wind() const = 0;
    virtual int res_petrify(bool temp = true) const = 0;
    virtual int res_constrict() const = 0;
    virtual int res_magic() const = 0;
    virtual int check_res_magic(int power);
    virtual bool no_tele(bool calc_unid = true, bool permit_id = true,
                         bool blink = false) const = 0;
    virtual bool inaccuracy() const;

    virtual bool gourmand(bool calc_unid = true, bool items = true) const;
    virtual bool conservation(bool calc_unid = true, bool items = true) const;
    virtual bool res_corr(bool calc_unid = true, bool items = true) const;
    bool has_notele_item(bool calc_unid = true) const;
    virtual bool stasis(bool calc_unid = true, bool items = true) const;
    virtual bool run(bool calc_unid = true, bool items = true) const;
    virtual bool angry(bool calc_unid = true, bool items = true) const;
    virtual bool clarity(bool calc_unid = true, bool items = true) const;
    virtual bool faith(bool calc_unid = true, bool items = true) const;
    virtual bool warding(bool calc_unid = true, bool items = true) const;
    virtual int archmagi(bool calc_unid = true, bool items = true) const;
    virtual bool no_cast(bool calc_unid = true, bool items = true) const;

    virtual bool rmut_from_item(bool calc_unid = true) const;
    virtual bool evokable_berserk(bool calc_unid = true) const;
    virtual bool evokable_invis(bool calc_unid = true) const;

    // Return an int so we know whether an item is the sole source.
    virtual int evokable_flight(bool calc_unid = true) const;
    virtual int evokable_jump(bool calc_unid = true) const;
    virtual int spirit_shield(bool calc_unid = true, bool items = true) const;

    virtual flight_type flight_mode() const = 0;
    virtual bool is_wall_clinging() const;
    virtual bool is_banished() const = 0;
    virtual bool can_cling_to_walls() const = 0;
    virtual bool can_cling_to(const coord_def& p) const;
    virtual bool check_clinging(bool stepped, bool door = false);
    virtual void clear_clinging();
    virtual bool is_web_immune() const = 0;
    virtual bool airborne() const;
    virtual bool ground_level() const;
    virtual bool stand_on_solid_ground() const;

    virtual bool paralysed() const = 0;
    virtual bool cannot_move() const = 0;
    virtual bool cannot_act() const = 0;
    virtual bool confused() const = 0;
    virtual bool caught() const = 0;
    virtual bool asleep() const { return false; }

    // check_haloed: include halo
    // self_halo: include own halo (actually if self_halo = false
    //            and has a halo, returns false; so if you have a
    //            halo you're not affected by others' halos for this
    //            purpose)
    virtual bool backlit(bool check_haloed = true,
                         bool self_halo = true) const = 0;
    virtual bool umbra(bool check_haloed = true,
                         bool self_halo = true) const = 0;
    // Within any actor's halo?
    virtual bool haloed() const;
    // Within an umbra?
    virtual bool umbraed() const;
    // Being heated by a heat aura?
    virtual bool heated() const;
    // Squared halo radius.
    virtual int halo_radius2() const = 0;
    // Squared silence radius.
    virtual int silence_radius2() const = 0;
    // Squared liquefying radius
    virtual int liquefying_radius2() const = 0;
    virtual int umbra_radius2() const = 0;
    virtual int heat_radius2() const = 0;

    virtual bool glows_naturally() const = 0;

    virtual bool petrifying() const = 0;
    virtual bool petrified() const = 0;

    virtual bool liquefied_ground() const = 0;

    virtual bool handle_trap();

    virtual void god_conduct(conduct_type thing_done, int level) { }

    virtual bool incapacitated() const
    {
        return cannot_move()
            || asleep()
            || confused()
            || caught()
            || petrifying();
    }

    virtual bool wont_attack() const = 0;
    virtual mon_attitude_type temp_attitude() const = 0;

    virtual bool has_spell(spell_type spell) const = 0;

    virtual bool     will_trigger_shaft() const;
    virtual level_id shaft_dest(bool known) const;
    virtual bool     do_shaft() = 0;

    coord_def position;

    CrawlHashTable props;

    int shield_blocks;                 // Count of shield blocks this round.

    // Constriction stuff:

    // What is holding us?  Not necessarily a monster.
    held_type held;
    mid_t constricted_by;
    int escape_attempts;

    // Map from mid to duration.
    typedef map<mid_t, int> constricting_t;
    // Freed and set to NULL when empty.
    constricting_t *constricting;

    void start_constricting(actor &whom, int duration = 0);

    void stop_constricting(mid_t whom, bool intentional = false,
                           bool quiet = false);
    void stop_constricting_all(bool intentional = false, bool quiet = false);
    void stop_being_constricted(bool quiet = false);

    bool can_constrict(actor* defender);
    void clear_far_constrictions();
    void accum_has_constricted();
    void handle_constriction();
    bool is_constricted() const;
    bool is_constricting() const;
    int num_constricting() const;
    virtual bool has_usable_tentacle() const = 0;
    virtual int constriction_damage() const = 0;

    // Be careful using this, as it doesn't keep the constrictor in sync.
    void clear_constricted();

    string describe_props() const;


protected:
    void end_constriction(constricting_t::iterator i, bool intentional,
                          bool quiet);
};

bool actor_slime_wall_immune(const actor *actor);

#endif
