#ifndef ACTOR_H
#define ACTOR_H

#include "itemprop-enum.h"
#include "random-var.h"
#include "ouch.h"

#define CLING_KEY "clinging" // 'is creature clinging' property key

enum ev_ignore_bit
{
    EV_IGNORE_NONE       = 0,
    EV_IGNORE_HELPLESS   = 1<<0,
    EV_IGNORE_UNIDED     = 1<<1,
};
DEF_BITFIELD(ev_ignore_type, ev_ignore_bit);

struct bolt;

class actor
{
public:
    virtual ~actor();

    monster_type type;
    mid_t        mid;
    virtual int       mindex() const = 0;

    virtual bool is_player() const = 0;
    bool is_monster() const { return !is_player(); }
    virtual monster* as_monster() = 0;
    virtual player* as_player() = 0;
    virtual const monster* as_monster() const = 0;
    virtual const player* as_player() const = 0;

    virtual kill_category kill_alignment() const = 0;
    virtual god_type  deity() const = 0;

    virtual bool      alive() const = 0;

    // Should return false for perma-summoned things.
    virtual bool is_summoned(int* duration = nullptr,
                             int* summon_type = nullptr) const = 0;

    virtual bool is_perm_summoned() const = 0;

    // [ds] Low-level moveto() - moves the actor without updating relevant
    // grids, such as mgrd.
    virtual void moveto(const coord_def &c, bool clear_net = true) = 0;

    // High-level actor movement. If in doubt, use this. Returns false if the
    // actor cannot be moved to the target, possibly because it is already
    // occupied.
    virtual bool move_to_pos(const coord_def &c, bool clear_net = true,
                             bool force = false) = 0;

    virtual void apply_location_effects(const coord_def &oldpos,
                                        killer_type killer = KILL_NONE,
                                        int killernum = -1) = 0;

    virtual void set_position(const coord_def &c);
    const coord_def& pos() const { return position; }

    virtual void self_destruct() { }

    // Blink the actor to the destination. c should be a
    // valid target, though the method returns false
    // if the blink fails.
    virtual bool blink_to(const coord_def &c, bool quiet = false) = 0;

    virtual bool      swimming() const = 0;
    virtual bool      submerged() const = 0;
    virtual bool      floundering() const = 0;

    // Returns true if the actor is exceptionally well balanced.
    virtual bool      extra_balanced() const = 0;

    virtual int       get_hit_dice() const = 0;
    virtual int       get_experience_level() const = 0;

    virtual bool shove(const char* feat_name = "") = 0;
    virtual bool can_pass_through_feat(dungeon_feature_type grid) const = 0;
    virtual bool can_pass_through(int x, int y) const;
    virtual bool can_pass_through(const coord_def &c) const;

    virtual bool is_habitable_feat(dungeon_feature_type actual_grid) const = 0;
            bool is_habitable(const coord_def &pos) const;

    virtual size_type body_size(size_part_type psize = PSIZE_TORSO,
                                bool base = false) const = 0;

    virtual brand_type damage_brand(int which_attack = -1) = 0;
    virtual int       damage_type(int which_attack = -1) = 0;
    virtual item_def *weapon(int which_attack = -1) const = 0;
    const item_def *primary_weapon() const
    {
        return weapon(0);
    }
    virtual random_var attack_delay(const item_def *projectile = nullptr,
                                    bool rescale = true) const = 0;
    virtual int has_claws(bool allow_tran = true) const = 0;
    virtual item_def *shield() const = 0;
    virtual item_def *slot_item(equipment_type eq,
                                bool include_melded=false) const = 0;
    virtual int wearing(equipment_type slot, int sub_type,
                        bool calc_unid = true) const = 0;
    virtual int wearing_ego(equipment_type slot, int sub_type,
                            bool calc_unid = true) const = 0;
    virtual int scan_artefacts(artefact_prop_type which_property,
                               bool calc_unid = true,
                               vector<item_def> *matches = nullptr) const = 0;

    virtual hands_reqd_type hands_reqd(const item_def &item,
                                       bool base = false) const;

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
                             bool ignore_transform = false,
                             bool quiet = true) const = 0;

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
                        bool force_visible = false,
                        bool force_article = false) const = 0;
    virtual string pronoun(pronoun_type which_pronoun,
                           bool force_visible = false) const = 0;
    virtual string conj_verb(const string &verb) const = 0;
    virtual string hand_name(bool plural, bool *can_plural = nullptr) const = 0;
    virtual string foot_name(bool plural, bool *can_plural = nullptr) const = 0;
    virtual string arm_name(bool plural, bool *can_plural = nullptr) const = 0;

    virtual bool fumbles_attack() = 0;

    virtual bool fights_well_unarmed(int heavy_armour_penalty)
    {
        return true;
    }
    virtual void attacking(actor *other, bool ranged = false) = 0;
    virtual bool can_go_berserk() const = 0;
    virtual bool go_berserk(bool intentional, bool potion = false) = 0;
    virtual bool berserk() const = 0;
    virtual bool can_see_invisible(bool calc_unid = true) const = 0;
    virtual bool invisible() const = 0;
    virtual bool nightvision() const = 0;
    virtual reach_type reach_range() const = 0;

    // Would looker be able to see the actor when in LOS?
    virtual bool visible_to(const actor *looker) const = 0;

    // Is the given cell within LOS of the actor?
    virtual bool see_cell(const coord_def &c) const;
    virtual bool see_cell_no_trans(const coord_def &c) const;

    // Can the actor actually see the target?
    virtual bool can_see(const actor &target) const;

    // Visibility as required by messaging. In usual play:
    //   Does the player know what's happening to the actor?
    bool observable() const;

    virtual bool is_icy() const = 0;
    virtual bool is_fiery() const = 0;
    virtual bool is_skeletal() const = 0;
    virtual bool can_mutate() const = 0;
    virtual bool can_safely_mutate(bool temp = true) const = 0;
    virtual bool can_polymorph() const = 0;
    virtual bool can_bleed(bool allow_tran = true) const = 0;
    virtual bool is_stationary() const = 0;
    virtual bool malmutate(const string &reason) = 0;
    virtual bool polymorph(int pow) = 0;
    virtual bool drain_exp(actor *agent, bool quiet = false, int pow = 15) = 0;
    virtual bool rot(actor *agent, int amount, bool quiet = false,
                     bool no_cleanup = false) = 0;
    virtual int  hurt(const actor *attacker, int amount,
                      beam_type flavour = BEAM_MISSILE,
                      kill_method_type kill_type = KILLED_BY_MONSTER,
                      string source = "",
                      string aux = "",
                      bool cleanup_dead = true,
                      bool attacker_effects = true) = 0;
    virtual bool heal(int amount) = 0;
    virtual void banish(actor *agent, const string &who = "",
                        const int power = 0, bool force = false) = 0;
    virtual void blink() = 0;
    virtual void teleport(bool right_now = false,
                          bool wizard_tele = false) = 0;
    virtual bool poison(actor *attacker, int amount = 1, bool force = false) = 0;
    virtual bool sicken(int amount) = 0;
    virtual void paralyse(actor *attacker, int strength, string source = "") = 0;
    virtual void petrify(actor *attacker, bool force = false) = 0;
    virtual bool fully_petrify(actor *foe, bool quiet = false) = 0;
    virtual void slow_down(actor *attacker, int strength) = 0;
    virtual void confuse(actor *attacker, int strength) = 0;
    virtual void put_to_sleep(actor *attacker, int strength,
                              bool hibernate = false) = 0;
    virtual void weaken(actor *attacker, int pow) = 0;
    virtual void expose_to_element(beam_type element, int strength = 0,
                                   bool slow_cold_blood = true) = 0;
    virtual void drain_stat(stat_type stat, int amount) { }
    virtual void splash_with_acid(const actor* evildoer, int acid_strength = -1,
                                  bool allow_corrosion = true,
                                  const char* hurt_msg = nullptr) = 0;
    virtual bool corrode_equipment(const char* corrosion_source = "the acid",
                                   int degree = 1) = 0;

    virtual bool can_hibernate(bool holi_only = false,
                               bool intrinsic_only = false) const;
    virtual bool can_sleep(bool holi_only = false) const;
    virtual void check_awaken(int disturbance) = 0;
    virtual int beam_resists(bolt &beam, int hurted, bool doEffects,
                             string source = "") = 0;

    virtual int  skill(skill_type sk, int scale = 1,
                       bool real = false, bool drained = true) const = 0;
    int  skill_rdiv(skill_type sk, int mult = 1, int div = 1) const;

#define TORPOR_SLOWED_KEY "torpor_slowed"
    bool torpor_slowed() const;

    virtual int heads() const = 0;

    virtual int stat_hp() const = 0;
    virtual int stat_maxhp() const = 0;

    virtual int stealth () const = 0;

    virtual bool can_throw_large_rocks() const = 0;

    virtual int armour_class(bool calc_unid = true) const = 0;
    virtual int gdr_perc() const = 0;
    int apply_ac(int damage, int max_damage = 0, ac_type ac_rule = AC_NORMAL,
                 int stab_bypass = 0, bool for_real = true) const;
    virtual int evasion(ev_ignore_type ign = EV_IGNORE_NONE,
                        const actor *attacker = nullptr) const = 0;
    virtual bool shielded() const = 0;
    virtual int shield_bonus() const = 0;
    virtual int shield_block_penalty() const = 0;
    virtual int shield_bypass_ability(int tohit) const = 0;
    virtual void shield_block_succeeded(actor *foe);
    virtual int missile_deflection() const = 0; // 1 = RMsl, 2 = DMsl
    virtual void ablate_deflection() = 0;

    // Combat-related virtual class methods
    virtual int unadjusted_body_armour_penalty() const = 0;
    virtual int adjusted_body_armour_penalty(int scale = 1) const = 0;
    virtual int adjusted_shield_penalty(int scale) const = 0;
    virtual int armour_tohit_penalty(bool random_factor, int scale = 1) const = 0;
    virtual int shield_tohit_penalty(bool random_factor, int scale = 1) const = 0;

    virtual monster_type mons_species(bool zombie_base = false) const = 0;

    virtual mon_holy_type holiness(bool temp = true) const = 0;
    virtual bool undead_or_demonic() const = 0;
    virtual bool holy_wrath_susceptible() const = 0;
    virtual bool is_holy(bool spells = true) const = 0;
    virtual bool is_nonliving(bool temp = true) const = 0;
    bool evil() const;
    virtual int  how_chaotic(bool check_spells_god = false) const = 0;
    virtual bool is_unbreathing() const = 0;
    virtual bool is_insubstantial() const = 0;
    virtual int res_acid(bool calc_unid = true) const = 0;
    virtual bool res_damnation() const = 0;
    virtual int res_fire() const = 0;
    virtual int res_steam() const = 0;
    virtual int res_cold() const = 0;
    virtual int res_elec() const = 0;
    virtual int res_poison(bool temp = true) const = 0;
    virtual int res_rotting(bool temp = true) const = 0;
    virtual int res_water_drowning() const = 0;
    virtual bool res_sticky_flame() const = 0;
    virtual int res_holy_energy(const actor *attacker) const = 0;
    virtual int res_negative_energy(bool intrinsic_only = false) const = 0;
    virtual bool res_torment() const = 0;
    virtual bool res_wind() const = 0;
    virtual bool res_petrify(bool temp = true) const = 0;
    virtual int res_constrict() const = 0;
    virtual int res_magic(bool calc_unid = true) const = 0;
    virtual int check_res_magic(int power);
    virtual bool no_tele(bool calc_unid = true, bool permit_id = true,
                         bool blink = false) const = 0;
    virtual int inaccuracy() const;
    virtual bool antimagic_susceptible() const = 0;

    virtual bool gourmand(bool calc_unid = true, bool items = true) const;

    virtual bool res_corr(bool calc_unid = true, bool items = true) const;
    bool has_notele_item(bool calc_unid = true,
                         vector<item_def> *matches = nullptr) const;
    virtual bool stasis(bool calc_unid = true, bool items = true) const;
    virtual bool run(bool calc_unid = true, bool items = true) const;
    virtual bool angry(bool calc_unid = true, bool items = true) const;
    virtual bool clarity(bool calc_unid = true, bool items = true) const;
    virtual bool faith(bool calc_unid = true, bool items = true) const;
    virtual int archmagi(bool calc_unid = true, bool items = true) const;
    virtual int spec_evoke(bool calc_unid = true, bool items = true) const;
    virtual bool no_cast(bool calc_unid = true, bool items = true) const;
    virtual bool reflection(bool calc_unid = true, bool items = true) const;
    virtual bool extra_harm(bool calc_unid = true, bool items = true) const;

    virtual bool rmut_from_item(bool calc_unid = true) const;
    virtual bool evokable_berserk(bool calc_unid = true) const;
    virtual bool evokable_invis(bool calc_unid = true) const;

    // Return an int so we know whether an item is the sole source.
    virtual int evokable_flight(bool calc_unid = true) const;
    virtual int spirit_shield(bool calc_unid = true, bool items = true) const;

    virtual bool is_wall_clinging() const;
    virtual bool is_banished() const = 0;
    virtual bool can_cling_to_walls() const = 0;
    virtual bool can_cling_to(const coord_def& p) const;
    virtual bool check_clinging(bool stepped, bool door = false);
    virtual void clear_clinging();
    virtual bool is_web_immune() const = 0;
    virtual bool airborne() const = 0;
    virtual bool ground_level() const;
    virtual bool stand_on_solid_ground() const;

    virtual bool paralysed() const = 0;
    virtual bool cannot_move() const = 0;
    virtual bool cannot_act() const = 0;
    virtual bool confused() const = 0;
    virtual bool caught() const = 0;
    virtual bool asleep() const { return false; }

    // self_halo: include own halo (actually if self_halo = false
    //            and has a halo, returns false; so if you have a
    //            halo you're not affected by others' halos for this
    //            purpose)
    virtual bool backlit(bool self_halo = true) const = 0;
    virtual bool umbra() const = 0;
    // Within any actor's halo?
    virtual bool haloed() const;
    // Within an umbra?
    virtual bool umbraed() const;
#if TAG_MAJOR_VERSION == 34
    // Being heated by a heat aura?
    virtual bool heated() const;
#endif
    // Halo radius.
    virtual int halo_radius() const = 0;
    // Silence radius.
    virtual int silence_radius() const = 0;
    // Liquefying radius.
    virtual int liquefying_radius() const = 0;
    virtual int umbra_radius() const = 0;
#if TAG_MAJOR_VERSION == 34
    virtual int heat_radius() const = 0;
#endif

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
    virtual mon_attitude_type real_attitude() const = 0;

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
    // Freed and set to nullptr when empty.
    constricting_t *constricting;

    void start_constricting(actor &whom, int duration = 0);

    void stop_constricting(mid_t whom, bool intentional = false,
                           bool quiet = false);
    void stop_constricting_all(bool intentional = false, bool quiet = false);
    void stop_being_constricted(bool quiet = false);

    bool can_constrict(const actor* defender) const;
    void clear_far_constrictions();
    void clear_constrictions_far_from(const coord_def &where);
    void accum_has_constricted();
    void handle_constriction();
    bool is_constricted() const;
    bool is_constricting() const;
    int num_constricting() const;
    virtual bool has_usable_tentacle() const = 0;
    virtual int constriction_damage() const = 0;
    virtual bool clear_far_engulf() = 0;

    // Be careful using this, as it doesn't keep the constrictor in sync.
    void clear_constricted();

    string describe_props() const;

    string resist_margin_phrase(int margin) const;

    void collide(coord_def newpos, const actor *agent, int pow);

private:
    void end_constriction(mid_t whom, bool intentional, bool quiet);
};

bool actor_slime_wall_immune(const actor *actor);

#endif
