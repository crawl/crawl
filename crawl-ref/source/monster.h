#ifndef MONSTER_H
#define MONSTER_H

#include "actor.h"
#include <stdint.h>

const int KRAKEN_TENTACLE_RANGE = 3;
#define TIDE_CALL_TURN "tide-call-turn"

#define MAX_DAMAGE_COUNTER 10000

class mon_enchant
{
public:
    enchant_type  ench;
    int           degree;
    int           duration, maxduration;
    kill_category who;      // Who set this enchantment?

public:
    mon_enchant(enchant_type e = ENCH_NONE, int deg = 0,
                kill_category whose = KC_OTHER,
                int dur = 0);

    killer_type killer() const;
    int kill_agent() const;

    operator std::string () const;
    const char *kill_category_desc(kill_category) const;
    void merge_killer(kill_category who);
    void cap_degree();

    void set_duration(const monster* mons, const mon_enchant *exist);

    bool operator < (const mon_enchant &other) const
    {
        return (ench < other.ench);
    }

    bool operator == (const mon_enchant &other) const
    {
        // NOTE: This does *not* check who/degree.
        return (ench == other.ench);
    }

    mon_enchant &operator += (const mon_enchant &other);
    mon_enchant operator + (const mon_enchant &other) const;

private:
    int modded_speed(const monster* mons, int hdplus) const;
    int calc_duration(const monster* mons, const mon_enchant *added) const;
};

typedef std::map<enchant_type, mon_enchant> mon_enchant_list;

struct monsterentry;

class monster : public actor
{
public:
    monster();
    monster(const monster& other);
    ~monster();

    monster& operator = (const monster& other);
    void reset();

public:
    // Possibly some of these should be moved into the hash table
    std::string mname;

    monster_type type;
    int hit_points;
    int max_hit_points;
    int hit_dice;
    int ac;
    int ev;
    int speed;
    int speed_increment;

    coord_def target;
    coord_def patrol_point;
    mutable montravel_target_type travel_target;
    std::vector<coord_def> travel_path;
    FixedVector<short, NUM_MONSTER_SLOTS> inv;
    monster_spells spells;
    mon_attitude_type attitude;
    beh_type behaviour;
    unsigned short foe;
    int8_t ench_countdown;
    mon_enchant_list enchantments;
    uint64_t flags;                    // bitfield of boolean flags

    unsigned int experience;
    monster_type  base_monster;        // zombie base monster, draconian colour
    unsigned int  number;              // #heads (hydra), etc.
    int           colour;

    int foe_memory;                    // how long to 'remember' foe x,y
                                       // once they go out of sight.

    int shield_blocks;                 // Count of shield blocks this round.

    god_type god;                      // What god the monster worships, if
                                       // any.

    std::auto_ptr<ghost_demon> ghost;  // Ghost information.

    std::string seen_context;          // Non-standard context for
                                       // AI_SEE_MONSTER

    int damage_friendly;               // Damage taken, x2 you, x1 pets, x0 else.
    int damage_total;

    CrawlHashTable props;

public:
    mon_attitude_type temp_attitude() const;

    // Returns true if the monster is named with a proper name, or is
    // a player ghost.
    bool is_named() const;

    // Does this monster have a base name, i.e. is base_name() != name().
    // See base_name() for details.
    bool has_base_name() const;

    const monsterentry *find_monsterentry() const;
    monster_type get_mislead_type() const;

    void init_experience();

    void mark_summoned(int longevity, bool mark_items_summoned,
                       int summon_type = 0);
    bool is_summoned(int* duration = NULL, int* summon_type = NULL) const;
    bool has_action_energy() const;
    void check_redraw(const coord_def &oldpos) const;
    void apply_location_effects(const coord_def &oldpos,
                                killer_type killer = KILL_NONE,
                                int killernum = -1);

    void moveto(const coord_def& c);
    bool move_to_pos(const coord_def &newpos);
    bool blink_to(const coord_def& c, bool quiet = false);

    kill_category kill_alignment() const;

    int  foe_distance() const;
    bool needs_berserk(bool check_spells = true) const;

    // Has a hydra-like variable number of attacks based on mons->number.
    bool has_hydra_multi_attack() const;
    bool has_multitargeting() const;

    // Has the 'spellcaster' flag (may not actually have any spells).
    bool can_use_spells() const;

    // Has the 'priest' flag.
    bool is_priest() const;

    // Is an actual wizard-type spellcaster (it can be silenced, Trog
    // will dislike it, etc.).
    bool is_actual_spellcaster() const;

    // Has ENCH_SHAPESHIFTER or ENCH_GLOWING_SHAPESHIFTER.
    bool is_shapeshifter() const;

    bool has_ench(enchant_type ench) const;
    bool has_ench(enchant_type ench, enchant_type ench2) const;
    mon_enchant get_ench(enchant_type ench,
                         enchant_type ench2 = ENCH_NONE) const;
    bool add_ench(const mon_enchant &);
    void update_ench(const mon_enchant &);
    bool del_ench(enchant_type ench, bool quiet = false, bool effect = true);
    bool lose_ench_duration(const mon_enchant &e, int levels);
    bool lose_ench_levels(const mon_enchant &e, int lev);
    void lose_energy(energy_use_type et, int div = 1, int mult = 1);

    void scale_hp(int num, int den);
    bool gain_exp(int exp);

    void react_to_damage(const actor *oppressor, int damage, beam_type flavour);

    void forget_random_spell();

    void add_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void remove_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void apply_enchantments();
    void apply_enchantment(const mon_enchant &me);

    bool can_drink_potion(potion_type ptype) const;
    bool should_drink_potion(potion_type ptype) const;
    item_type_id_state_type drink_potion_effect(potion_type pot_eff);

    void timeout_enchantments(int levels);

    bool is_travelling() const;
    bool is_patrolling() const;
    bool needs_transit() const;
    void set_transit(const level_id &destination);
    bool find_place_to_live(bool near_player = false);
    bool find_home_near_place(const coord_def &c);
    bool find_home_near_player();
    bool find_home_anywhere();

    void set_ghost(const ghost_demon &ghost, bool has_name = true);
    void ghost_init();
    void pandemon_init();
    void dancing_weapon_init();
    void uglything_init(bool only_mutate = false);
    void uglything_mutate(uint8_t force_colour = BLACK);
    void uglything_upgrade();
    void destroy_inventory();
    void load_spells(mon_spellbook_type spellbook);

    actor *get_foe() const;

    // actor interface
    monster_type id() const;
    int mindex() const;
    int       get_experience_level() const;
    god_type  deity() const;
    bool      alive() const;
    bool      defined() const { return alive(); }
    bool      swimming() const;
    bool      wants_submerge() const;

    bool      submerged() const;
    bool      can_drown() const;
    bool      floundering() const;
    bool      extra_balanced() const;
    bool      has_trachea() const;
    bool      can_pass_through_feat(dungeon_feature_type grid) const;
    bool      is_habitable_feat(dungeon_feature_type actual_grid) const;
    size_type body_size(size_part_type psize = PSIZE_TORSO,
                        bool base = false) const;
    int       body_weight(bool base = false) const;
    int       total_weight() const;
    int       damage_brand(int which_attack = -1);
    int       damage_type(int which_attack = -1);

    item_def *slot_item(equipment_type eq, bool include_melded=false);
    item_def *mslot_item(mon_inv_type sl) const;
    item_def *weapon(int which_attack = -1);
    item_def *launcher();
    item_def *missiles();
    item_def *shield();

    bool      can_wield(const item_def &item,
                        bool ignore_curse = false,
                        bool ignore_brand = false,
                        bool ignore_shield = false,
                        bool ignore_transform = false) const;
    bool      could_wield(const item_def &item,
                          bool ignore_brand = false,
                          bool ignore_transform = false) const;

    int       missile_count();
    void      wield_melee_weapon(int near = -1);
    void      swap_weapons(int near = -1);

    bool      pickup_item(item_def &item, int near = -1, bool force = false);
    void      pickup_message(const item_def &item, int near);
    bool      pickup_wand(item_def &item, int near);
    bool      pickup_scroll(item_def &item, int near);
    bool      pickup_potion(item_def &item, int near);
    bool      pickup_gold(item_def &item, int near);
    bool      pickup_launcher(item_def &launcher, int near, bool force = false);
    bool      pickup_melee_weapon(item_def &item, int near);
    bool      pickup_throwable_weapon(item_def &item, int near);
    bool      pickup_weapon(item_def &item, int near, bool force);
    bool      pickup_armour(item_def &item, int near, bool force);
    bool      pickup_misc(item_def &item, int near);
    bool      pickup_missile(item_def &item, int near, bool force);
    void      equip(item_def &item, int slot, int near = -1);
    bool      unequip(item_def &item, int slot, int near = -1,
                      bool force = false);

    bool      can_use_missile(const item_def &item) const;

    std::string name(description_level_type type,
                     bool force_visible = false) const;

    // Base name of the monster, bypassing any mname setting. For an orc priest
    // named Arbolt, name() will return "Arbolt", but base_name() will return
    // "orc priest".
    std::string base_name(description_level_type type,
                          bool force_visible = false) const;
    // Full name of the monster.  For an orc priest named Arbolt, full_name()
    // will return "Arbolt the orc priest".
    std::string full_name(description_level_type type,
                          bool use_comma = false) const;
    std::string pronoun(pronoun_type pro, bool force_visible = false) const;
    std::string conj_verb(const std::string &verb) const;
    std::string hand_name(bool plural, bool *can_plural = NULL) const;
    std::string foot_name(bool plural, bool *can_plural = NULL) const;
    std::string arm_name(bool plural, bool *can_plural = NULL) const;

    bool fumbles_attack(bool verbose = true);
    bool cannot_fight() const;

    int  skill(skill_type skill, bool skill_bump = false) const;

    void attacking(actor *other);
    bool can_go_berserk() const;
    void go_berserk(bool intentional, bool potion = false);
    void go_frenzy();
    bool berserk() const;
    bool frenzied() const;
    bool can_mutate() const;
    bool can_safely_mutate() const;
    bool can_bleed() const;
    bool mutate();
    void banish(const std::string &who = "");
    void expose_to_element(beam_type element, int strength = 0);

    int mons_species() const;

    mon_holy_type holiness() const;
    bool undead_or_demonic() const;
    bool is_holy() const;
    bool is_unholy() const;
    bool is_evil() const;
    bool is_unclean() const;
    bool is_known_chaotic() const;
    bool is_chaotic() const;
    bool is_artificial() const;
    bool is_unbreathing() const;
    bool is_insubstantial() const;
    int res_hellfire() const;
    int res_fire() const;
    int res_steam() const;
    int res_cold() const;
    int res_elec() const;
    int res_poison() const;
    int res_rotting() const;
    int res_asphyx() const;
    int res_water_drowning() const;
    int res_sticky_flame() const;
    int res_holy_energy(const actor *) const;
    int res_negative_energy() const;
    int res_torment() const;
    int res_acid() const;
    int res_magic() const;

    flight_type flight_mode() const;
    bool is_levitating() const;
    bool invisible() const;
    bool can_see_invisible() const;
    bool visible_to(const actor *looker) const;
    bool near_foe() const;
    reach_type reach_range() const;

    bool is_icy() const;
    bool is_fiery() const;
    bool is_skeletal() const;
    bool paralysed() const;
    bool cannot_move() const;
    bool cannot_act() const;
    bool confused() const;
    bool confused_by_you() const;
    bool caught() const;
    bool asleep() const;
    bool backlit(bool check_haloed = true, bool self_halo = true) const;
    int halo_radius2() const;
    int silence_radius2() const;
    bool glows_naturally() const;
    bool petrified() const;
    bool petrifying() const;

    bool friendly() const;
    bool neutral() const;
    bool good_neutral() const;
    bool strict_neutral() const;
    bool wont_attack() const;
    bool pacified() const;
    bool withdrawn() const {return has_ench(ENCH_WITHDRAWN);};

    bool has_spells() const;
    bool has_spell(spell_type spell) const;
    bool has_holy_spell() const;
    bool has_unholy_spell() const;
    bool has_evil_spell() const;
    bool has_unclean_spell() const;
    bool has_chaotic_spell() const;

    bool has_attack_flavour(int flavour) const;
    bool has_damage_type(int dam_type);

    bool can_throw_large_rocks() const;
    bool can_speak();

    int armour_class() const;
    int melee_evasion(const actor *attacker, ev_ignore_type evit) const;

    void poison(actor *agent, int amount = 1);
    bool sicken(int strength);
    bool bleed(int amount, int degree);
    void paralyse(actor *, int str);
    void petrify(actor *, int str);
    void slow_down(actor *, int str);
    void confuse(actor *, int strength);
    bool drain_exp(actor *, bool quiet = false, int pow = 3);
    bool rot(actor *, int amount, int immediate = 0, bool quiet = false);
    int hurt(const actor *attacker, int amount,
             beam_type flavour = BEAM_MISSILE,
             bool cleanup_dead = true);
    bool heal(int amount, bool max_too = false);
    void blink(bool allow_partial_control = true);
    void teleport(bool right_now = false,
                  bool abyss_shift = false,
                  bool wizard_tele = false);

    void hibernate(int power = 0);
    void put_to_sleep(actor *attacker, int power = 0);
    void check_awaken(int disturbance);

    int stat_hp() const    { return hit_points; }
    int stat_maxhp() const { return max_hit_points; }

    int shield_bonus() const;
    int shield_block_penalty() const;
    void shield_block_succeeded(actor *foe);
    int shield_bypass_ability(int tohit) const;

    actor_type atype() const { return ACT_MONSTER; }
    monster* as_monster() { return this; }
    player* as_player() { return NULL; }
    const monster* as_monster() const { return this; }
    const player* as_player() const { return NULL; }

    // Hacks, with a capital H.
    void fix_speed();
    void check_speed();
    void upgrade_type(monster_type after, bool adjust_hd, bool adjust_hp);

    std::string describe_enchantments() const;

    int action_energy(energy_use_type et) const;

    bool do_shaft();
    bool has_spell_of_type(unsigned) const;

    void bind_melee_flags();
    void bind_spell_flags();

private:
    void init_with(const monster& mons);
    void swap_slots(mon_inv_type a, mon_inv_type b);
    bool need_message(int &near) const;
    bool level_up();
    bool level_up_change();
    bool pickup(item_def &item, int slot, int near, bool force_merge = false);
    void equip_weapon(item_def &item, int near, bool msg = true);
    void equip_armour(item_def &item, int near);
    void unequip_weapon(item_def &item, int near, bool msg = true);
    void unequip_armour(item_def &item, int near);

    bool decay_enchantment(const mon_enchant &me, bool decay_degree = true);

    bool drop_item(int eslot, int near);
    bool wants_weapon(const item_def &item) const;
    bool wants_armour(const item_def &item) const;
    void lose_pickup_energy();
    bool check_set_valid_home(const coord_def &place,
                              coord_def &chosen,
                              int &nvalid) const;
};

#endif
