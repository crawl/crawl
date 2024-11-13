#pragma once

#include <vector>

#include "artefact-prop-type.h"
#include "beam-type.h"
#include "conduct-type.h"
#include "constrict-type.h"
#include "energy-use-type.h"
#include "equipment-type.h"
#include "god-type.h"
#include "item-prop-enum.h"
#include "kill-method-type.h"
#include "mon-holy-type.h"
#include "random-var.h"
#include "ouch.h"
#include "pronoun-type.h"
#include "reach-type.h"
#include "size-part-type.h"
#include "size-type.h"
#include "stat-type.h"

using std::vector;

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
    virtual bool is_summoned() const = 0;
    virtual bool was_created_by(int summon_type) const = 0;
    virtual bool was_created_by(const actor& summoner,
                                int summon_type = SPELL_NO_SPELL) const = 0;

    virtual bool is_firewood() const = 0;
    virtual bool is_peripheral() const = 0;

    // [ds] Low-level moveto() - moves the actor without updating relevant
    // grids, such as env.mgrid.
    virtual void moveto(const coord_def &c, bool clear_net = true,
                        bool clear_constrict = true) = 0;

    // High-level actor movement. If in doubt, use this. Returns false if the
    // actor cannot be moved to the target, possibly because it is already
    // occupied.
    virtual bool move_to_pos(const coord_def &c, bool clear_net = true,
                             bool force = false, bool clear_constrict = true) = 0;

    virtual void apply_location_effects(const coord_def &oldpos,
                                        killer_type killer = KILL_NONE,
                                        int killernum = -1) = 0;

    // Handles sticky flame / barbs on deliberate movement.
    virtual void did_deliberate_movement() = 0;

    virtual void set_position(const coord_def &c);
    const coord_def& pos() const { return position; }

    virtual void self_destruct() { }

    // Blink the actor to the destination. c should be a
    // valid target, though the method returns false
    // if the blink fails.
    virtual bool blink_to(const coord_def &c, bool quiet = false) = 0;

    virtual bool      swimming() const = 0;
    virtual bool      floundering() const = 0;

    // Returns true if the actor is exceptionally well balanced.
    virtual bool      extra_balanced() const = 0;

    virtual int       get_hit_dice() const = 0;
    virtual int       get_experience_level() const = 0;

    virtual bool shove(const char* feat_name = "") = 0;
    virtual bool can_pass_through_feat(dungeon_feature_type grid) const = 0;
    virtual bool can_pass_through(int x, int y) const;
    virtual bool can_pass_through(const coord_def &c) const;
    virtual bool can_burrow() const = 0;

    virtual bool is_habitable_feat(dungeon_feature_type actual_grid) const = 0;
            bool is_habitable(const coord_def &pos) const;

    virtual size_type body_size(size_part_type psize = PSIZE_TORSO,
                                bool base = false) const = 0;

    virtual brand_type damage_brand(int which_attack = -1) = 0;
    virtual vorpal_damage_type damage_type(int which_attack = -1) = 0;
    virtual item_def *weapon(int which_attack = -1) const = 0;
    const item_def *primary_weapon() const
    {
        return weapon(0);
    }
    virtual item_def *offhand_weapon() const { return nullptr; }
    virtual random_var attack_delay(const item_def *projectile = nullptr,
                                    bool rescale = true) const = 0;
    virtual int has_claws(bool allow_tran = true) const = 0;
    virtual item_def *shield() const = 0;
    virtual item_def *slot_item(equipment_type eq,
                                bool include_melded=false) const = 0;
    virtual int wearing(equipment_type slot, int sub_type) const = 0;
    virtual int wearing_ego(equipment_type slot, int sub_type) const = 0;
    virtual int scan_artefacts(artefact_prop_type which_property,
                               vector<const item_def *> *matches = nullptr) const = 0;

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

    virtual void attacking(actor *other) = 0;
    virtual bool can_go_berserk() const = 0;
    virtual bool go_berserk(bool intentional, bool potion = false) = 0;
    virtual bool berserk() const = 0;
    virtual bool can_see_invisible() const = 0;
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
    virtual bool has_blood(bool temp = true) const = 0;
    virtual bool has_bones(bool temp = true) const = 0;
    virtual bool is_stationary() const = 0;
    virtual bool malmutate(const string &reason) = 0;
    virtual bool polymorph(int pow, bool allow_immobile = true) = 0;
    virtual bool drain(const actor *agent, bool quiet = false,
                       int pow = 3) = 0;
    virtual int  hurt(const actor *attacker, int amount,
                      beam_type flavour = BEAM_MISSILE,
                      kill_method_type kill_type = KILLED_BY_MONSTER,
                      string source = "",
                      string aux = "",
                      bool cleanup_dead = true,
                      bool attacker_effects = true) = 0;
    virtual bool heal(int amount) = 0;
    virtual void banish(const actor *agent, const string &who = "",
                        const int power = 0, bool force = false) = 0;
    virtual void blink() = 0;
    virtual void teleport(bool right_now = false,
                          bool wizard_tele = false) = 0;
    virtual bool poison(actor *attacker, int amount = 1, bool force = false) = 0;
    virtual bool sicken(int amount) = 0;
    virtual void paralyse(const actor *attacker, int strength,
                          string source = "") = 0;
    virtual void petrify(const actor *attacker, bool force = false) = 0;
    virtual bool fully_petrify(bool quiet = false) = 0;
    virtual void slow_down(actor *attacker, int strength) = 0;
    virtual void confuse(actor *attacker, int strength) = 0;
    virtual void put_to_sleep(actor *attacker, int strength,
                              bool hibernate = false) = 0;
    virtual void weaken(const actor *attacker, int pow) = 0;
    virtual bool strip_willpower(actor *attacker, int dur,
                                 bool quiet = false) = 0;
    virtual void expose_to_element(beam_type element, int strength = 0,
                                   bool slow_cold_blood = true) = 0;
    virtual void drain_stat(stat_type /*stat*/, int /*amount*/) { }
    virtual void splash_with_acid(actor *evildoer) = 0;
    virtual void acid_corrode(int acid_strength) = 0;
    virtual bool corrode_equipment(const char* corrosion_source = "the acid",
                                   int degree = 1) = 0;
    virtual bool resists_dislodge(string /*event*/ = "") const { return false; };

    virtual bool can_hibernate(bool holi_only = false,
                               bool intrinsic_only = false) const;
    virtual bool can_sleep(bool holi_only = false) const;
    virtual void check_awaken(int disturbance) = 0;
    virtual int beam_resists(bolt &beam, int hurted, bool doEffects,
                             string source = "") = 0;
    virtual bool can_feel_fear(bool include_unknown) const = 0;

    virtual int  skill(skill_type sk, int scale = 1, bool real = false,
                       bool temp = true) const = 0;
    int  skill_rdiv(skill_type sk, int mult = 1, int div = 1) const;

    virtual int heads() const = 0;

    virtual int stat_hp() const = 0;
    virtual int stat_maxhp() const = 0;

    virtual int stealth () const = 0;

    virtual bool can_throw_large_rocks() const = 0;

    virtual bool can_be_dazzled() const = 0;
    virtual bool can_be_blinded() const = 0;

    virtual int armour_class() const = 0;
    virtual int gdr_perc() const = 0;
    int apply_ac(int damage, int max_damage = 0,
                 ac_type ac_rule = ac_type::normal,
                 bool for_real = true) const;
    virtual int evasion(bool ignore_temporary = false,
                        const actor *attacker = nullptr) const = 0;
    virtual bool shielded() const = 0;
    virtual int shield_block_limit() const;
    bool shield_exhausted() const;
    virtual int shield_bonus() const = 0;
    virtual int shield_bypass_ability(int tohit) const = 0;
    virtual void shield_block_succeeded(actor *attacker);
    virtual bool missile_repulsion() const = 0;

    virtual monster_type mons_species(bool zombie_base = false) const = 0;

    virtual mon_holy_type holiness(bool temp = true, bool incl_form = true) const = 0;
    virtual bool undead_or_demonic(bool temp = true) const = 0;
    virtual bool holy_wrath_susceptible() const;
    virtual bool is_holy() const = 0;
    virtual bool is_nonliving(bool temp = true, bool incl_form = true) const = 0;
    virtual bool evil() const;
    virtual int  how_chaotic(bool check_spells_god = false) const = 0;
    virtual bool is_unbreathing() const = 0;
    virtual bool is_insubstantial() const = 0;
    virtual bool is_amorphous() const = 0;
    virtual int res_acid() const = 0;
    virtual bool res_damnation() const = 0;
    virtual int res_fire() const = 0;
    virtual int res_steam() const = 0;
    virtual int res_cold() const = 0;
    virtual int res_elec() const = 0;
    virtual int res_poison(bool temp = true) const = 0;
    virtual bool res_miasma(bool temp = true) const = 0;
    virtual bool res_water_drowning() const = 0;
    virtual bool res_sticky_flame() const = 0;
    virtual int res_holy_energy() const = 0;
    virtual int res_foul_flame() const = 0;
    virtual int res_negative_energy(bool intrinsic_only = false) const = 0;
    virtual bool res_torment() const = 0;
    virtual bool res_polar_vortex() const = 0;
    virtual bool res_petrify(bool temp = true) const = 0;
    virtual bool res_constrict() const = 0;
    int get_res(int res) const;
    virtual int willpower() const = 0;
    virtual int check_willpower(const actor* source, int power) const;
    virtual bool no_tele(bool blink = false, bool temp = true) const = 0;
    virtual int inaccuracy() const;
    int inaccuracy_penalty() const;
    virtual bool antimagic_susceptible() const = 0;

    virtual bool res_corr(bool /*allow_random*/ = true, bool temp = true) const;
    bool has_notele_item(vector<const item_def *> *matches = nullptr) const;
    virtual bool stasis() const = 0;
    virtual bool cloud_immune(bool items = true) const;
    virtual int  angry(bool items = true) const;
    virtual bool clarity(bool items = true) const;
    virtual bool faith(bool items = true) const;
    virtual int archmagi(bool items = true) const;
    virtual bool no_cast(bool items = true) const;
    virtual bool reflection(bool items = true) const;
    virtual int extra_harm(bool items = true) const;

    virtual bool rmut_from_item() const;
    virtual bool evokable_invis() const;

    // Return an int so we know whether an item is the sole source.
    virtual int equip_flight() const;
    virtual int spirit_shield(bool items = true) const;
    virtual bool rampaging() const;

    virtual bool is_banished() const = 0;
    virtual bool is_web_immune() const = 0;
    virtual bool is_binding_sigil_immune() const = 0;
    virtual bool airborne() const = 0;
    virtual bool ground_level() const;

    virtual bool is_dragonkind() const;
    virtual int  dragon_level() const;

    virtual bool paralysed() const = 0;
    virtual bool cannot_act() const = 0;
    virtual bool confused() const = 0;
    virtual bool caught() const = 0;
    virtual bool asleep() const { return false; }

    // self_halo: include own halo (actually if self_halo = false
    //            and has a halo, returns false; so if you have a
    //            halo you're not affected by others' halos for this
    //            purpose)
    virtual bool backlit(bool self_halo = true, bool temp = true) const = 0;
    virtual bool umbra() const = 0;
    // Within any actor's halo?
    virtual bool haloed() const;
    // Within an umbra?
    virtual bool umbraed() const;
    // Halo radius.
    virtual int halo_radius() const = 0;
    // Silence radius.
    virtual int silence_radius() const = 0;
    // Demonspawn silence radius
    virtual int demon_silence_radius() const = 0;
    // Liquefying radius.
    virtual int liquefying_radius() const = 0;
    virtual int umbra_radius() const = 0;

    virtual bool petrifying() const = 0;
    virtual bool petrified() const = 0;

    virtual bool liquefied_ground() const = 0;

    virtual bool handle_trap();

    virtual void god_conduct(conduct_type /*thing_done*/, int /*level*/) { }

    virtual bool incapacitated() const
    {
        return cannot_act()
            || asleep()
            || confused()
            || caught();
    }

    virtual bool wont_attack() const = 0;
    virtual mon_attitude_type temp_attitude() const = 0;
    virtual mon_attitude_type real_attitude() const = 0;

    virtual bool has_spell(spell_type spell) const = 0;

    virtual bool     will_trigger_shaft() const;
    virtual level_id shaft_dest() const;
    virtual bool     do_shaft() = 0;

    coord_def position;

    CrawlHashTable props;

    int shield_blocks;                 // Count of shield blocks this round.
    bool triggered_spectral;           // Triggered spectral weapon this round

    // Constriction stuff:

    mid_t constricted_by;
    int escape_attempts;

    // mids of all actors we are constricting.
    // Freed and set to nullptr when empty.
    vector<mid_t> *constricting;

    void start_constricting(actor &whom);

    void stop_constricting(mid_t whom, bool intentional = false,
                           bool quiet = false, const string& escape_verb = "");
    void stop_constricting_all(bool intentional = false, bool quiet = false);
    void stop_directly_constricting_all(bool intentional = false,
                                        bool quiet = false);
    void stop_being_constricted(bool quiet = false, const string& escape_verb = "");

    virtual bool attempt_escape() = 0;

    bool can_constrict(const actor &defender, constrict_type typ) const;
    bool can_engulf(const actor &defender) const;
    bool has_invalid_constrictor(bool move = false) const;
    void clear_invalid_constrictions(bool move = false);
    void handle_constriction();
    bool is_constricted() const;
    constrict_type get_constrict_type() const;
    bool is_constricting() const;
    bool is_constricting(const actor &victim) const;
    int num_constricting() const;
    virtual bool has_usable_tentacle() const = 0;
    virtual int constriction_damage(constrict_type typ) const = 0;
    virtual bool clear_far_engulf(bool force = false, bool moved = false) = 0;

    // Be careful using this, as it doesn't keep the constrictor in sync.
    void clear_constricted();

    string describe_props() const;

    string resist_margin_phrase(int margin) const;

    void collide(coord_def newpos, const actor *agent, int damage);
    bool knockback(const actor &cause, int dist, int pow, string source_name);
    coord_def stumble_pos(coord_def targ) const;
    void stumble_away_from(coord_def targ, string src = "");

    static const actor *ensure_valid_actor(const actor *act);
    static actor *ensure_valid_actor(actor *act);

private:
    void constriction_damage_defender(actor &defender);
    void end_constriction(mid_t whom, bool intentional, bool quiet,
                          const string& escape_verb = "");
};

bool actor_slime_wall_immune(const actor *actor);
