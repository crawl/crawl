#ifndef ACTOR_H
#define ACTOR_H

#include "los_def.h"

enum ev_ignore_type
{
    EV_IGNORE_NONE       = 0,
    EV_IGNORE_HELPLESS   = 1,
    EV_IGNORE_PHASESHIFT = 2,
};

class actor
{
public:
    virtual ~actor();

    virtual monster_type  id() const = 0;
    virtual int       mindex() const = 0;
    virtual actor_type atype() const = 0;

    virtual monsters* as_monster() = 0;
    virtual player* as_player() = 0;
    virtual const monsters* as_monster() const = 0;
    virtual const player* as_player() const = 0;

    virtual kill_category kill_alignment() const = 0;
    virtual god_type  deity() const = 0;

    virtual bool      alive() const = 0;

    virtual bool is_summoned(int* duration = NULL,
                             int* summon_type = NULL) const = 0;

    // [ds] Low-level moveto() - moves the actor without updating relevant
    // grids, such as mgrd.
    virtual void moveto(const coord_def &c) = 0;

    // High-level actor movement. If in doubt, use this. Returns false if the
    // actor cannot be moved to the target, possibly because it is already
    // occupied.
    virtual bool move_to_pos(const coord_def &c) = 0;

    virtual void apply_location_effects(const coord_def &oldpos,
                                        killer_type killer = KILL_NONE,
                                        int killernum = -1) = 0;

    virtual void set_position(const coord_def &c);
    virtual const coord_def& pos() const { return position; }

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

    virtual bool can_pass_through_feat(dungeon_feature_type grid) const = 0;
    virtual bool can_pass_through(int x, int y) const;
    virtual bool can_pass_through(const coord_def &c) const;

    virtual bool is_habitable_feat(dungeon_feature_type actual_grid) const = 0;
            bool is_habitable(const coord_def &pos) const;

    virtual size_type body_size(size_part_type psize = PSIZE_TORSO,
                                bool base = false) const = 0;
    virtual int       body_weight(bool base = false) const;
    virtual int       total_weight() const = 0;

    virtual int       damage_brand(int which_attack = -1) = 0;
    virtual int       damage_type(int which_attack = -1) = 0;
    virtual item_def *weapon(int which_attack = -1) = 0;
    // Yay for broken overloading.
    const item_def *primary_weapon() const
    {
        return const_cast<actor*>(this)->weapon(0);
    }
    virtual item_def *shield() = 0;
    virtual item_def *slot_item(equipment_type eq,
                                bool include_melded=false) = 0;
    // Just a wrapper; not to be overridden
    const item_def *slot_item(equipment_type eq,
                              bool include_melded=false) const
    {
        return const_cast<actor*>(this)->slot_item(eq, include_melded);
    }
    virtual bool has_equipped(equipment_type eq, int sub_type) const;

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

    virtual int hunger_level() const { return HS_ENGORGED; }
    virtual void make_hungry(int nutrition, bool silent = true)
    {
    }

    // Need not be implemented for the player - player action costs
    // are explicitly calculated.
    virtual void lose_energy(energy_use_type, int div = 1, int mult = 1)
    {
    }

    virtual std::string name(description_level_type type,
                             bool force_visible = false) const = 0;
    virtual std::string pronoun(pronoun_type which_pronoun,
                                bool force_visible = false) const = 0;
    virtual std::string conj_verb(const std::string &verb) const = 0;
    virtual std::string hand_name(bool plural,
                                  bool *can_plural = NULL) const = 0;
    virtual std::string foot_name(bool plural,
                                  bool *can_plural = NULL) const = 0;
    virtual std::string arm_name(bool plural,
                                 bool *can_plural = NULL) const = 0;

    virtual bool fumbles_attack(bool verbose = true) = 0;

    // Returns true if the actor has no way to attack (plants, statues).
    // (statues have only indirect attacks).
    virtual bool cannot_fight() const = 0;
    virtual void attacking(actor *other) = 0;
    virtual bool can_go_berserk() const = 0;
    virtual void go_berserk(bool intentional, bool potion = false) = 0;
    virtual bool berserk() const = 0;
    virtual bool can_see_invisible() const = 0;
    virtual bool invisible() const = 0;

    // Would looker be able to see the actor when in LOS?
    virtual bool visible_to(const actor *looker) const = 0;

    // Is the given cell within LOS of the actor?
    virtual bool see_cell(const coord_def &c) const;
    virtual bool see_cell_no_trans(const coord_def &c) const;

    virtual const los_base* get_los();
    virtual const los_base* get_los_no_trans();

    // Can the actor actually see the target?
    virtual bool can_see(const actor *target) const;

    // Visibility as required by messaging. In usual play:
    //   Does the player know what's happening to the actor?
    virtual bool observable() const;

    virtual bool is_icy() const = 0;
    virtual bool is_fiery() const = 0;
    virtual bool is_skeletal() const = 0;
    virtual bool can_mutate() const = 0;
    virtual bool can_safely_mutate() const = 0;
    virtual bool can_bleed() const = 0;
    virtual bool mutate() = 0;
    virtual bool drain_exp(actor *agent, bool quiet = false, int pow = 3) = 0;
    virtual bool rot(actor *agent, int amount, int immediate = 0,
                     bool quiet = false) = 0;
    virtual int  hurt(const actor *attacker, int amount,
                      beam_type flavour = BEAM_MISSILE,
                      bool cleanup_dead = true) = 0;
    virtual bool heal(int amount, bool max_too = false) = 0;
    virtual void banish(const std::string &who = "") = 0;
    virtual void blink(bool allow_partial_control = true) = 0;
    virtual void teleport(bool right_now = false,
                          bool abyss_shift = false,
                          bool wizard_tele = false) = 0;
    virtual void poison(actor *attacker, int amount = 1) = 0;
    virtual bool sicken(int amount) = 0;
    virtual void paralyse(actor *attacker, int strength) = 0;
    virtual void petrify(actor *attacker, int strength) = 0;
    virtual void slow_down(actor *attacker, int strength) = 0;
    virtual void confuse(actor *attacker, int strength) = 0;
    virtual void put_to_sleep(actor *attacker, int strength) = 0;
    virtual void expose_to_element(beam_type element, int strength = 0) = 0;
    virtual void drain_stat(stat_type stat, int amount, actor* attacker) { }
    virtual bool can_hibernate(bool holi_only = false) const;
    virtual bool can_sleep() const;
    virtual void hibernate(int power = 0) = 0;
    virtual void check_awaken(int disturbance) = 0;

    virtual int  skill(skill_type sk, bool skill_bump = false) const
    {
        return (0);
    }

    virtual int stat_hp() const = 0;
    virtual int stat_maxhp() const = 0;

    virtual bool can_throw_large_rocks() const = 0;

    virtual int armour_class() const = 0;
    virtual int melee_evasion(const actor *attacker,
                              ev_ignore_type ign = EV_IGNORE_NONE) const = 0;
    virtual int shield_bonus() const = 0;
    virtual int shield_block_penalty() const = 0;
    virtual int shield_bypass_ability(int tohit) const = 0;

    virtual void shield_block_succeeded(actor *foe);

    virtual int mons_species() const = 0;

    virtual mon_holy_type holiness() const = 0;
    virtual bool undead_or_demonic() const = 0;
    virtual bool is_holy() const = 0;
    virtual bool is_unholy() const = 0;
    virtual bool is_evil() const = 0;
    virtual bool is_chaotic() const = 0;
    virtual bool is_insubstantial() const = 0;
    virtual int res_acid() const = 0;
    virtual int res_fire() const = 0;
    virtual int res_steam() const = 0;
    virtual int res_cold() const = 0;
    virtual int res_elec() const = 0;
    virtual int res_poison() const = 0;
    virtual int res_rotting() const = 0;
    virtual int res_asphyx() const = 0;
    virtual int res_water_drowning() const = 0;
    virtual int res_sticky_flame() const = 0;
    virtual int res_holy_energy(const actor *attacker) const = 0;
    virtual int res_negative_energy() const = 0;
    virtual int res_torment() const = 0;
    virtual int res_magic() const = 0;
    virtual bool check_res_magic(int power);

    virtual flight_type flight_mode() const = 0;
    virtual bool is_levitating() const = 0;
    virtual bool airborne() const;

    virtual bool paralysed() const = 0;
    virtual bool cannot_move() const = 0;
    virtual bool cannot_act() const = 0;
    virtual bool confused() const = 0;
    virtual bool caught() const = 0;
    virtual bool asleep() const { return (false); }

    // check_haloed: include halo
    // self_halo: include own halo (actually if self_halo = false
    //            and has a halo, returns false; so if you have a
    //            halo you're not affected by others' halos for this
    //            purpose)
    virtual bool backlit(bool check_haloed = true,
                         bool self_halo = true) const = 0;
    // Within any actor's halo?
    virtual bool haloed() const;
    // Squared halo radius.
    virtual int halo_radius2() const = 0;
    // Squared silence radius.
    virtual int silence_radius2() const = 0;

    virtual bool petrified() const = 0;

    virtual bool handle_trap();

    virtual void god_conduct(conduct_type thing_done, int level) { }

    virtual bool incapacitated() const
    {
        return cannot_move() || asleep() || confused() || caught();
    }

    virtual int warding() const
    {
        return (0);
    }

    virtual bool has_spell(spell_type spell) const = 0;

    virtual bool     will_trigger_shaft() const;
    virtual level_id shaft_dest(bool known) const;
    virtual bool     do_shaft() = 0;

    coord_def position;

protected:
    // These are here for memory management reasons...
    los_glob los;
    los_glob los_no_trans;
};

// Identical to actor->kill_alignment(), but returns KC_OTHER if the actor
// is NULL.
kill_category actor_kill_alignment(const actor *actor);
bool actor_slime_wall_immune(const actor *actor);

#endif
