#ifndef MONSTER_H
#define MONSTER_H

#include "actor.h"
#include "mon-ench.h"

const int KRAKEN_TENTACLE_RANGE = 3;
#define TIDE_CALL_TURN "tide-call-turn"

#define MAX_DAMAGE_COUNTER 10000

typedef map<enchant_type, mon_enchant> mon_enchant_list;

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
    string mname;

    int hit_points;
    int max_hit_points;
    int hit_dice;
    int ac;
    int ev;
    int speed;
    int speed_increment;

    coord_def target;
    coord_def firing_pos;
    coord_def patrol_point;
    mutable montravel_target_type travel_target;
    vector<coord_def> travel_path;
    FixedVector<short, NUM_MONSTER_SLOTS> inv;
    monster_spells spells;
    mon_attitude_type attitude;
    beh_type behaviour;
    unsigned short foe;
    int8_t ench_countdown;
    mon_enchant_list enchantments;
    FixedBitVector<NUM_ENCHANTMENTS> ench_cache;
    uint64_t flags;                    // bitfield of boolean flags

    unsigned int experience;
    monster_type  base_monster;        // zombie base monster, draconian colour
    unsigned int  number;              // #heads (hydra), etc.
    int           colour;
    mid_t         summoner;

    int foe_memory;                    // how long to 'remember' foe x,y
                                       // once they go out of sight.

    god_type god;                      // What god the monster worships, if
                                       // any.

    unique_ptr<ghost_demon> ghost;     // Ghost information.

    seen_context_type seen_context;    // Non-standard context for
                                       // AI_SEE_MONSTER

    int damage_friendly;               // Damage taken, x2 you, x1 pets, x0 else.
    int damage_total;

    uint32_t client_id;                // for ID of monster_info between turns
    static uint32_t last_client_id;

public:
    void set_new_monster_id();

    uint32_t get_client_id() const;
    void reset_client_id();
    void ensure_has_client_id();

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
                       int summon_type = 0, bool abj = true);
    bool is_summoned(int* duration = NULL, int* summon_type = NULL) const;
    bool is_perm_summoned() const;
    bool has_action_energy() const;
    void check_redraw(const coord_def &oldpos, bool clear_tiles = true) const;
    void apply_location_effects(const coord_def &oldpos,
                                killer_type killer = KILL_NONE,
                                int killernum = -1);
    bool self_destructs();

    void moveto(const coord_def& c, bool clear_net = true);
    bool move_to_pos(const coord_def &newpos, bool clear_net = true);
    bool blink_to(const coord_def& c, bool quiet = false);
    bool blink_to(const coord_def& c, bool quiet, bool jump);
    kill_category kill_alignment() const;

    int  foe_distance() const;
    bool needs_berserk(bool check_spells = true) const;

    // Has a hydra-like variable number of attacks based on mons->number.
    bool has_hydra_multi_attack() const;
    bool has_multitargetting() const;

    // Has the 'spellcaster' flag (may not actually have any spells).
    bool can_use_spells() const;

    // Has the 'priest' flag.
    bool is_priest() const;

    // Has the 'fighter' flag.
    bool is_fighter() const;

    // Has the 'archer' flag.
    bool is_archer() const;

    // Is an actual wizard-type spellcaster (it can be silenced, Trog
    // will dislike it, etc.).
    bool is_actual_spellcaster() const;

    // Has ENCH_SHAPESHIFTER or ENCH_GLOWING_SHAPESHIFTER.
    bool is_shapeshifter() const;

#ifdef DEBUG_DIAGNOSTICS
    bool has_ench(enchant_type ench) const; // same but validated
#else
    bool has_ench(enchant_type ench) const { return ench_cache[ench]; }
#endif
    bool has_ench(enchant_type ench, enchant_type ench2) const;
    mon_enchant get_ench(enchant_type ench,
                         enchant_type ench2 = ENCH_NONE) const;
    bool add_ench(const mon_enchant &);
    void update_ench(const mon_enchant &);
    bool del_ench(enchant_type ench, bool quiet = false, bool effect = true);
    bool lose_ench_duration(const mon_enchant &e, int levels);
    bool lose_ench_levels(const mon_enchant &e, int lev);
    void lose_energy(energy_use_type et, int div = 1, int mult = 1);
    void gain_energy(energy_use_type et, int div = 1, int mult = 1);

    void scale_hp(int num, int den);
    bool gain_exp(int exp, int max_levels_to_gain = 2);

    void react_to_damage(const actor *oppressor, int damage, beam_type flavour);

    void forget_random_spell();

    void add_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void remove_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void apply_enchantments();
    void apply_enchantment(const mon_enchant &me);

    bool can_drink_potion(potion_type ptype) const;
    bool should_drink_potion(potion_type ptype) const;
    item_type_id_state_type drink_potion_effect(potion_type pot_eff);

    bool can_evoke_jewellery(jewellery_type jtype) const;
    bool should_evoke_jewellery(jewellery_type jtype) const;
    item_type_id_state_type evoke_jewellery_effect(jewellery_type jtype);

    void timeout_enchantments(int levels);

    bool is_travelling() const;
    bool is_patrolling() const;
    bool needs_abyss_transit() const;
    void set_transit(const level_id &destination);
    bool find_place_to_live(bool near_player = false);
    bool find_home_near_place(const coord_def &c);
    bool find_home_near_player();
    bool find_home_anywhere();

    // The map that placed this monster.
    bool has_originating_map() const;
    string originating_map() const;
    void set_originating_map(const string &);

    void set_ghost(const ghost_demon &ghost);
    void ghost_init(bool need_pos = true);
    void ghost_demon_init();
    void uglything_init(bool only_mutate = false);
    void uglything_mutate(colour_t force_colour = BLACK);
    void uglything_upgrade();
    void destroy_inventory();
    void load_ghost_spells();

    actor *get_foe() const;

    // actor interface
    int mindex() const;
    int       get_experience_level() const;
    god_type  deity() const;
    bool      alive() const;
    bool      defined() const { return alive(); }
    bool      swimming() const;
    bool      wants_submerge() const;

    bool        submerged() const;
    bool        can_drown() const;
    bool        floundering_at(const coord_def p) const;
    bool        floundering() const;
    bool        extra_balanced_at(const coord_def p) const;
    bool        extra_balanced() const;
    bool        can_pass_through_feat(dungeon_feature_type grid) const;
    bool        is_habitable_feat(dungeon_feature_type actual_grid) const;
    bool        shove(const char* name = "");
    size_type   body_size(size_part_type psize = PSIZE_TORSO,
                          bool base = false) const;
    int         body_weight(bool base = false) const;
    int         total_weight() const;
    brand_type  damage_brand(int which_attack = -1);
    int         damage_type(int which_attack = -1);
    int         has_claws(bool allow_tran = true) const;

    int wearing(equipment_type slot, int type, bool calc_unid = true) const;
    int wearing_ego(equipment_type slot, int type, bool calc_unid = true) const;
    int scan_artefacts(artefact_prop_type which_property,
                       bool calc_unid = true) const;

    item_def *slot_item(equipment_type eq, bool include_melded=false) const;
    item_def *mslot_item(mon_inv_type sl) const;
    item_def *weapon(int which_attack = -1) const;
    item_def *launcher();
    item_def *missiles();
    item_def *shield() const;

    hands_reqd_type hands_reqd(const item_def &item) const;

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
    bool      pickup_missile(item_def &item, int near);
    bool      pickup_weapon(item_def &item, int near, bool force);
    bool      pickup_armour(item_def &item, int near, bool force);
    bool      pickup_jewellery(item_def &item, int near, bool force);
    bool      pickup_misc(item_def &item, int near);
    bool      pickup_food(item_def &item, int near);
    bool      pickup_missile(item_def &item, int near, bool force);
    bool      drop_item(int eslot, int near);
    void      equip(item_def &item, int slot, int near = -1);
    bool      unequip(item_def &item, int slot, int near = -1,
                      bool force = false);
    void      steal_item_from_player();

    bool      can_use_missile(const item_def &item) const;

    string name(description_level_type type, bool force_visible = false) const;

    // Base name of the monster, bypassing any mname setting. For an orc priest
    // named Arbolt, name() will return "Arbolt", but base_name() will return
    // "orc priest".
    string base_name(description_level_type type,
                     bool force_visible = false) const;
    // Full name of the monster.  For an orc priest named Arbolt, full_name()
    // will return "Arbolt the orc priest".
    string full_name(description_level_type type, bool use_comma = false) const;
    string pronoun(pronoun_type pro, bool force_visible = false) const;
    string conj_verb(const string &verb) const;
    string hand_name(bool plural, bool *can_plural = NULL) const;
    string foot_name(bool plural, bool *can_plural = NULL) const;
    string arm_name(bool plural, bool *can_plural = NULL) const;

    bool fumbles_attack(bool verbose = true);
    bool cannot_fight() const;

    int  skill(skill_type skill, int scale = 1, bool real = false) const;

    void attacking(actor *other);
    bool can_go_frenzy() const;
    bool can_go_berserk() const;
    bool can_jump() const;
    void go_berserk(bool intentional, bool potion = false);
    bool go_frenzy(actor *source);
    bool berserk() const;
    bool berserk_or_insane() const;
    bool has_lifeforce() const;
    bool can_mutate() const;
    bool can_safely_mutate() const;
    bool can_polymorph() const;
    bool can_bleed(bool allow_tran = true) const;
    bool is_stationary() const;
    bool malmutate(const string &reason);
    bool polymorph(int pow);
    void banish(actor *agent, const string &who = "");
    void expose_to_element(beam_type element, int strength = 0,
                           bool damage_inventory = true,
                           bool slow_cold_blood = true);

    monster_type mons_species(bool zombie_base = false) const;

    mon_holy_type holiness() const;
    bool undead_or_demonic() const;
    bool is_holy(bool check_spells = true) const;
    bool is_unholy(bool check_spells = true) const;
    bool is_evil(bool check_spells = true) const;
    bool is_unclean(bool check_spells = true) const;
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
    int res_poison(bool temp = true) const;
    int res_rotting(bool temp = true) const;
    int res_asphyx() const;
    int res_water_drowning() const;
    int res_sticky_flame() const;
    int res_holy_energy(const actor *) const;
    int res_negative_energy(bool intrinsic_only = false) const;
    int res_torment() const;
    int res_acid(bool calc_unid = true) const;
    int res_wind() const;
    int res_petrify(bool temp = true) const;
    int res_constrict() const;
    int res_magic() const;
    bool no_tele(bool calc_unid = true, bool permit_id = true,
                 bool blink = false) const;
    bool res_corr(bool calc_unid = true, bool items = true) const;

    bool stasis(bool calc_unid = true, bool items = true) const;

    flight_type flight_mode() const;
    bool can_cling_to_walls() const;
    bool is_banished() const;
    bool is_web_immune() const;
    bool invisible() const;
    bool can_see_invisible() const;
    bool visible_to(const actor *looker) const;
    bool near_foe() const;
    reach_type reach_range() const;
    bool nightvision() const;

    bool is_icy() const;
    bool is_fiery() const;
    bool is_skeletal() const;
    int spiny_degree() const;
    bool paralysed() const;
    bool cannot_move() const;
    bool cannot_act() const;
    bool confused() const;
    bool confused_by_you() const;
    bool caught() const;
    bool asleep() const;
    bool backlit(bool check_haloed = true, bool self_halo = true) const;
    bool umbra(bool check_haloed = true, bool self_halo = true) const;
    int halo_radius2() const;
    int silence_radius2() const;
    int liquefying_radius2() const;
    int umbra_radius2() const;
    int suppression_radius2() const;
    int heat_radius2() const;
    bool glows_naturally() const;
    bool petrified() const;
    bool petrifying() const;
    bool liquefied_ground() const;

    bool friendly() const;
    bool neutral() const;
    bool good_neutral() const;
    bool strict_neutral() const;
    bool wont_attack() const;
    bool pacified() const;
    bool withdrawn() const {return has_ench(ENCH_WITHDRAWN);};

    bool rolling() const { return has_ench(ENCH_ROLLING); } ;
    bool has_spells() const;
    bool has_spell(spell_type spell) const;
    bool has_unholy_spell() const;
    bool has_evil_spell() const;
    bool has_unclean_spell() const;
    bool has_chaotic_spell() const;
    bool has_corpse_violating_spell() const;

    bool has_attack_flavour(int flavour) const;
    bool has_damage_type(int dam_type);
    int constriction_damage() const;

    bool can_throw_large_rocks() const;
    bool can_speak();

    int armour_class() const;
    int gdr_perc() const { return 0; }
    int melee_evasion(const actor *attacker, ev_ignore_type evit) const;

    bool poison(actor *agent, int amount = 1, bool force = false);
    bool sicken(int strength, bool unused = true, bool quiet = false);
    bool bleed(const actor *agent, int amount, int degree);
    void paralyse(actor *, int str, string source = "");
    void petrify(actor *, bool force = false);
    bool fully_petrify(actor *foe, bool quiet = false);
    void slow_down(actor *, int str);
    void confuse(actor *, int strength);
    bool drain_exp(actor *, bool quiet = false, int pow = 3);
    bool rot(actor *, int amount, int immediate = 0, bool quiet = false);
    int hurt(const actor *attacker, int amount,
             beam_type flavour = BEAM_MISSILE,
             bool cleanup_dead = true,
             bool attacker_effects = true);
    bool heal(int amount, bool max_too = false);
    void blame_damage(const actor *attacker, int amount);
    void blink(bool allow_partial_control = true);
    void teleport(bool right_now = false,
                  bool abyss_shift = false,
                  bool wizard_tele = false);
    void suicide(int hp = -1);

    void hibernate(int power = 0);
    void put_to_sleep(actor *attacker, int power = 0);
    void weaken(actor *attacker, int pow);
    void check_awaken(int disturbance);
    int beam_resists(bolt &beam, int hurted, bool doEffects, string source = "");

    int stat_hp() const    { return hit_points; }
    int stat_maxhp() const { return max_hit_points; }
    int stealth() const;

    int     shield_bonus() const;
    int     shield_block_penalty() const;
    void    shield_block_succeeded(actor *foe);
    int     shield_bypass_ability(int tohit) const;
    int     missile_deflection() const;

    // Combat-related class methods
    int     unadjusted_body_armour_penalty() const { return 0; }
    int     adjusted_body_armour_penalty(int, bool) const { return 0; }
    int     adjusted_shield_penalty(int) const { return 0; }
    int     armour_tohit_penalty(bool, int) const { return 0; }
    int     shield_tohit_penalty(bool, int) const { return 0; }

    actor_type atype() const { return ACT_MONSTER; }
    monster* as_monster() { return this; }
    player* as_player() { return NULL; }
    const monster* as_monster() const { return this; }
    const player* as_player() const { return NULL; }

    // Hacks, with a capital H.
    void check_speed();
    void upgrade_type(monster_type after, bool adjust_hd, bool adjust_hp);

    string describe_enchantments() const;

    int action_energy(energy_use_type et) const;

    bool do_shaft();
    bool has_spell_of_type(unsigned) const;

    void bind_melee_flags();
    void bind_spell_flags();
    void calc_speed();
    bool attempt_escape(int attempts = 1);
    bool has_usable_tentacle() const;

    bool check_clarity(bool silent) const;
    bool check_stasis(bool silent, bool calc_unid = true) const;

    bool is_child_tentacle() const;
    bool is_child_tentacle_of(const monster* mons) const;
    bool has_child_tentacles() const;
    bool is_child_monster() const;
    bool is_parent_monster_of(const monster* mons) const;
    bool is_child_tentacle_segment() const;

    bool is_divine_companion() const;
    bool is_projectile() const;
    // Jumping spiders (jump instead of blink)
    bool is_jumpy() const;

private:
    void init_with(const monster& mons);
    void swap_slots(mon_inv_type a, mon_inv_type b);
    bool need_message(int &near) const;
    bool level_up();
    bool level_up_change();
    bool pickup(item_def &item, int slot, int near);
    void equip_weapon(item_def &item, int near, bool msg = true);
    void equip_armour(item_def &item, int near);
    void equip_jewellery(item_def &item, int near);
    void unequip_weapon(item_def &item, int near, bool msg = true);
    void unequip_armour(item_def &item, int near);
    void unequip_jewellery(item_def &item, int near);
    int armour_bonus(const item_def &item);

    void id_if_worn(mon_inv_type mslot, object_class_type base_type,
                    int sub_type) const;

    bool decay_enchantment(enchant_type en, bool decay_degree = true);

    bool wants_weapon(const item_def &item) const;
    bool wants_armour(const item_def &item) const;
    bool wants_jewellery(const item_def &item) const;
    void lose_pickup_energy();
    bool check_set_valid_home(const coord_def &place,
                              coord_def &chosen,
                              int &nvalid) const;
};

#endif
