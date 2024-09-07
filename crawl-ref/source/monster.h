#pragma once

#include <functional>
#include <vector>

#include "actor.h"
#include "beh-type.h"
#include "enchant-type.h"
#include "kill-method-type.h"
#include "mon-ench.h"
#include "mon-poly.h"
#include "montravel-target-type.h"
#include "potion-type.h"
#include "seen-context-type.h"
#include "spl-util.h"
#include "xp-tracking-type.h"

using std::vector;

#define TIDE_CALL_TURN "tide-call-turn"

#define MAX_DAMAGE_COUNTER 10000
#define ZOMBIE_BASE_AC_KEY "zombie_base_ac"
#define ZOMBIE_BASE_EV_KEY "zombie_base_ev"
#define MON_SPEED_KEY "speed"
#define CUSTOM_SPELLS_KEY "custom_spells"
#define CUSTOM_SPELL_LIST_KEY "custom_spell_list"
#define SEEN_SPELLS_KEY "seen_spells"
#define KNOWN_MAX_HP_KEY "known_max_hp"
#define VAULT_HD_KEY "vault_hd"

#define FAKE_BLINK_KEY "fake_blink"
#define CEREBOV_DISARMED_KEY "cerebov_disarmed"

#define TENTACLE_LORD_HITS "tentacle_lord_hits_key"

/// has a given hound already used up its howl?
#define DOOM_HOUND_HOWLED_KEY "doom_hound_howled"
#define KIKU_WRETCH_KEY "kiku_wretch"

#define DROPPER_MID_KEY "dropper_mid"

#define MAP_KEY "map"

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
    monster_flags_t flags;             // bitfield of boolean flags
    xp_tracking_type xp_tracking;

    monster_type  base_monster;        // zombie base monster, draconian colour
    union
    {
        // These must all be the same size!
        unsigned int number;   ///< General purpose number variable
        int blob_size;         ///< num of slimes/masses in this one
        int num_heads;         ///< Hydra-like head number
        int ballisto_activity; ///< How active is this ballistomycete?
        int spore_cooldown;    ///< Can this make ballistos (if 0)
        int mangrove_pests;    ///< num of animals in shambling mangrove
        int prism_charge;      ///< Turns this prism has existed
        int battlecharge;      ///< Charges of battlesphere
        int move_spurt;        ///< Sixfirhy/jiangshi/kraken black magic
        int steps_remaining;   ///< Foxfire remaining moves
        int blazeheart_heat;   ///< Number of checks before golem cools
        mid_t tentacle_connect;///< mid of monster this tentacle is
                               //   connected to: for segments, this is the
                               //   tentacle; for tentacles, the head.
    };
    int           colour;
    mid_t         summoner;

    int foe_memory;                    // how long to 'remember' foe x,y
                                       // once they go out of sight.

    god_type god;                      // What god the monster worships, if
                                       // any.

    unique_ptr<ghost_demon> ghost;     // Ghost information.

    seen_context_type seen_context;    // Non-standard context for
                                       // activity_interrupt::see_monster

    int damage_friendly;               // Damage taken by a player-related source
                                       // (used for XP calculations)
    int damage_total;

    uint32_t client_id;                // for ID of monster_info between turns
    static uint32_t last_client_id;

    bool went_unseen_this_turn;
    coord_def unseen_pos;

public:
    void set_new_monster_id();

    uint32_t get_client_id() const;
    void reset_client_id();
    void ensure_has_client_id();

    void set_hit_dice(int new_hd);

    mon_attitude_type temp_attitude() const override;
    mon_attitude_type real_attitude() const override { return attitude; }

    // Returns true if the monster is named with a proper name, or is
    // a player ghost.
    bool is_named() const;

    // Does this monster have a base name, i.e. is base_name() != name().
    // See base_name() for details.
    bool has_base_name() const;

    const monsterentry *find_monsterentry() const;

    void mark_summoned(int longevity, bool mark_items_summoned,
                       int summon_type = 0, bool abj = true);
    bool is_summoned(int* duration = nullptr, int* summon_type = nullptr) const
        override;
    bool is_perm_summoned() const override;
    bool has_action_energy() const;
    void drain_action_energy();
    bool matches_player_speed() const;
    int  player_speed_energy() const;
    void check_redraw(const coord_def &oldpos, bool clear_tiles = true) const;
    void apply_location_effects(const coord_def &oldpos,
                                killer_type killer = KILL_NONE,
                                int killernum = -1) override;
    void did_deliberate_movement() override;
    void self_destruct() override;

    void set_position(const coord_def &c) override;
    void moveto(const coord_def& c, bool clear_net = true) override;
    bool move_to_pos(const coord_def &newpos, bool clear_net = true,
                     bool force = false) override;
    bool swap_with(monster* other);
    bool blink_to(const coord_def& c, bool quiet = false) override;
    bool blink_to(const coord_def& c, bool quiet, bool jump);
    kill_category kill_alignment() const override;

    int  foe_distance() const;
    bool needs_berserk(bool check_spells = true, bool ignore_distance = false) const;

    // Has a hydra-like variable number of attacks based on num_heads.
    bool has_hydra_multi_attack() const;
    int  heads() const override;
    bool has_multitargeting() const;

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

#ifdef DEBUG_ENCH_CACHE_DIAGNOSTICS
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
    bool lose_ench_levels(const mon_enchant &e, int lev, bool infinite = false);
    void lose_energy(energy_use_type et, int div = 1, int mult = 1);
    int energy_cost(energy_use_type et, int div = 1, int mult = 1) const;

    void scale_hp(int num, int den);

    void react_to_damage(const actor *oppressor, int damage, beam_type flavour,
                         kill_method_type ktype);

    void apply_enchantments();

    void timeout_enchantments(int levels);

    bool is_travelling() const;
    bool is_patrolling() const;
    bool needs_abyss_transit() const;
    void set_transit(const level_id &destination);
    bool is_trap_safe(const coord_def& where) const;
    bool is_location_safe(const coord_def &place);
    bool find_place_to_live(bool near_player = false, bool force_near = false);
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
    void inugami_init();
    void uglything_init(bool only_mutate = false);
    void uglything_mutate(colour_t force_colour = COLOUR_UNDEF);
    void destroy_inventory();
    void load_ghost_spells();
    brand_type ghost_brand() const;
    bool has_ghost_brand() const;
    int ghost_umbra_radius() const;

    actor *get_foe() const;

    // actor interface
    int mindex() const override;
    int      get_hit_dice() const override;
    int      get_experience_level() const override;
    god_type deity() const override;
    bool     alive() const override;
    bool     defined() const { return alive(); }
    bool     swimming() const override;

    bool     can_drown() const;
    bool     floundering_at(const coord_def p) const;
    bool     floundering() const override;
    bool     extra_balanced_at(const coord_def p) const;
    bool     extra_balanced() const override;
    bool     can_pass_through_feat(dungeon_feature_type grid) const override;
    bool     can_burrow() const override;
    bool     can_burrow_through(dungeon_feature_type feat) const;
    bool     is_habitable_feat(dungeon_feature_type actual_grid) const override;
    bool     shove(const char* name = "") override;

    size_type   body_size(size_part_type psize = PSIZE_TORSO,
                          bool base = false) const override;
    brand_type  damage_brand(int which_attack = -1) override;
    vorpal_damage_type damage_type(int which_attack = -1) override;
    random_var  attack_delay(const item_def *projectile = nullptr,
                             bool rescale = true) const override;
    int         has_claws(bool allow_tran = true) const override;

    int wearing(equipment_type slot, int type) const override;
    int wearing_ego(equipment_type slot, int type) const override;
    int scan_artefacts(artefact_prop_type which_property,
                       vector<const item_def *> *_unused_matches = nullptr) const
        override;

    item_def *slot_item(equipment_type eq, bool include_melded=false) const
        override;
    item_def *mslot_item(mon_inv_type sl) const;
    item_def *weapon(int which_attack = -1) const override;
    item_def *launcher() const;
    item_def *melee_weapon() const;
    item_def *missiles() const;
    item_def *shield() const override;
    item_def *get_defining_object() const;

    hands_reqd_type hands_reqd(const item_def &item,
                               bool base = false) const override;

    bool      can_wield(const item_def &item,
                        bool ignore_curse = false,
                        bool ignore_brand = false,
                        bool ignore_shield = false,
                        bool ignore_transform = false) const override;
    bool      could_wield(const item_def &item,
                          bool ignore_brand = false,
                          bool ignore_transform = false,
                          bool quiet = true) const override;

    void      wield_melee_weapon(maybe_bool msg = maybe_bool::maybe);
    void      swap_weapons(maybe_bool msg = maybe_bool::maybe);
    bool      pickup_item(item_def &item, bool msg, bool force);
    bool      drop_item(mon_inv_type eslot, bool msg);
    bool      unequip(item_def &item, bool msg, bool force = false);
    void      steal_item_from_player();
    item_def* take_item(int steal_what, mon_inv_type mslot,
                        bool is_stolen = false);
    item_def* disarm();

    bool      can_use_missile(const item_def &item) const;
    bool      likes_wand(const item_def &item) const;

    string name(description_level_type type, bool force_visible = false,
                bool force_article = false) const override;

    // Base name of the monster, bypassing any mname setting. For an orc priest
    // named Arbolt, name() will return "Arbolt", but base_name() will return
    // "orc priest".
    string base_name(description_level_type type,
                     bool force_visible = false) const;
    // Full name of the monster. For an orc priest named Arbolt, full_name()
    // will return "Arbolt the orc priest".
    string full_name(description_level_type type) const;
    string pronoun(pronoun_type pro, bool force_visible = false) const override;
    bool pronoun_plurality(bool force_visible = false) const;
    string conj_verb(const string &verb) const override;
    string hand_name(bool plural, bool *can_plural = nullptr) const override;
    string foot_name(bool plural, bool *can_plural = nullptr) const override;
    string arm_name(bool plural, bool *can_plural = nullptr) const override;

    bool fumbles_attack() override;

    int  skill(skill_type skill, int scale = 1, bool real = false,
               bool temp = true) const override;

    void attacking(actor *other) override;
    bool can_go_frenzy() const;
    bool can_go_berserk() const override;
    bool can_get_mad() const;
    bool go_berserk(bool intentional, bool potion = false) override;
    bool go_frenzy(actor *source);
    bool berserk() const override;
    bool berserk_or_frenzied() const;
    bool can_mutate() const override;
    bool can_safely_mutate(bool temp = true) const override;
    bool can_polymorph() const override;
    bool has_blood(bool temp = true) const override;
    bool has_bones(bool temp = true) const override;
    bool is_stationary() const override;
    bool malmutate(const string &/*reason*/) override;
    void corrupt();
    bool polymorph(int pow, bool allow_immobile = true) override;
    bool polymorph(poly_power_type power = PPT_SAME);
    void banish(const actor *agent, const string &who = "", const int power = 0,
                bool force = false) override;
    void expose_to_element(beam_type element, int strength = 0,
                           bool slow_cold_blood = true) override;

    monster_type mons_species(bool zombie_base = false) const override;

    mon_holy_type holiness(bool /*temp*/ = true, bool /*incl_form*/ = true) const override;
    bool undead_or_demonic(bool /*temp*/ = true) const override;
    bool evil() const override;
    bool is_holy() const override;
    bool is_nonliving(bool /*temp*/ = true, bool /*incl_form*/ = true) const override;
    int how_unclean(bool check_god = true) const;
    int known_chaos(bool check_spells_god = false) const;
    int how_chaotic(bool check_spells_god = false) const override;
    bool is_unbreathing() const override;
    bool is_insubstantial() const override;
    bool is_amorphous() const override;
    bool res_damnation() const override;
    int res_fire() const override;
    int res_steam() const override;
    int res_cold() const override;
    int res_elec() const override;
    int res_poison(bool temp = true) const override;
    bool res_miasma(bool /*temp*/ = true) const override;
    bool res_water_drowning() const override;
    bool res_sticky_flame() const override;
    int res_holy_energy() const override;
    int res_foul_flame() const override;
    int res_negative_energy(bool intrinsic_only = false) const override;
    bool res_torment() const override;
    int res_acid() const override;
    bool res_polar_vortex() const override;
    bool res_petrify(bool /*temp*/ = true) const override;
    bool res_constrict() const override;
    resists_t all_resists() const;
    int willpower() const override;
    bool no_tele(bool blink = false, bool /*temp*/ = true) const override;
    bool res_corr(bool /*allow_random*/ = true, bool temp = true) const override;
    bool antimagic_susceptible() const override;

    bool stasis() const override;
    bool cloud_immune(bool items = true) const override;

    bool airborne() const override;
    bool is_banished() const override;
    bool is_web_immune() const override;
    bool is_binding_sigil_immune() const override;
    bool invisible() const override;
    bool can_see_invisible() const override;
    bool visible_to(const actor *looker) const override;
    bool near_foe() const;
    reach_type reach_range() const override;
    bool nightvision() const override;

    bool is_icy() const override;
    bool is_fiery() const override;
    bool is_skeletal() const override;
    bool is_spiny() const;
    bool paralysed() const override;
    bool cannot_act() const override;
    bool confused() const override;
    bool confused_by_you() const;
    bool caught() const override;
    bool asleep() const override;
    bool sleepwalking() const;
    bool unswappable() const;
    bool backlit(bool self_halo = true, bool /*temp*/ = true) const override;
    bool umbra() const override;
    int halo_radius() const override;
    int silence_radius() const override;
    int demon_silence_radius() const override;
    int liquefying_radius() const override;
    int umbra_radius() const override;
    bool petrified() const override;
    bool petrifying() const override;
    bool liquefied_ground() const override;
    int natural_regen_rate() const;
    int off_level_regen_rate() const;
    bool can_feel_fear(bool include_unknown) const override;

    bool friendly() const;
    bool neutral() const;
    bool good_neutral() const;
    bool wont_attack() const override;
    bool pacified() const;

    bool has_spells() const;
    bool has_spell(spell_type spell) const override;
    mon_spell_slot_flags spell_slot_flags(spell_type spell) const;
    bool has_unclean_spell() const;
    bool has_chaotic_spell() const;
    bool immune_to_silence() const;

    bool has_attack_flavour(int flavour) const;
    bool has_damage_type(int dam_type);
    int constriction_damage(constrict_type typ) const override;

    bool can_throw_large_rocks() const override;

    bool can_be_dazzled() const override;
    bool can_be_blinded() const override;

    bool can_speak();
    bool is_silenced() const;

    int base_armour_class() const;
    int armour_class() const override;
    int gdr_perc() const override { return 0; }
    int base_evasion() const;
    int evasion(bool ignore_temporary = false,
                const actor* /*attacker*/ = nullptr) const override;

    bool poison(actor *agent, int amount = 1, bool force = false) override;
    bool sicken(int strength) override;
    void paralyse(const actor *, int str, string source = "") override;
    void petrify(const actor *, bool force = false) override;
    bool fully_petrify(bool quiet = false) override;
    void slow_down(actor *, int str) override;
    void confuse(actor *, int strength) override;
    bool drain(const actor *, bool quiet = false, int pow = 3) override;
    void splash_with_acid(actor *evildoer) override;
    void acid_corrode(int /*acid_strength*/) override;
    bool corrode_equipment(const char* corrosion_source = "the acid",
                           int degree = 1) override;
    int hurt(const actor *attacker, int amount,
             beam_type flavour = BEAM_MISSILE,
             kill_method_type kill_type = KILLED_BY_MONSTER,
             string source = "",
             string aux = "",
             bool cleanup_dead = true,
             bool attacker_effects = true) override;
    bool heal(int amount) override;
    void blame_damage(const actor *attacker, int amount);
    void blink() override;
    void teleport(bool right_now = false,
                  bool wizard_tele = false) override;
    bool shift(coord_def p = coord_def(0, 0));
    void suicide(int hp_target = -1);

    void put_to_sleep(actor *attacker, int power = 0, bool hibernate = false)
        override;
    void weaken(const actor *attacker, int pow) override;
    bool strip_willpower(actor *attacker, int dur, bool quiet = false) override;
    void check_awaken(int disturbance) override;
    int beam_resists(bolt &beam, int hurted, bool doEffects, string source = "")
        override;

    int stat_hp() const override    { return hit_points; }
    int stat_maxhp() const override { return max_hit_points; }
    int stealth() const override { return 0; }


    bool    shielded() const override;
    int     shield_class() const;
    int     shield_bonus() const override;
    void    shield_block_succeeded(actor *attacker) override;
    int     shield_bypass_ability(int tohit) const override;
    bool    missile_repulsion() const override;

    bool is_player() const override { return false; }
    monster* as_monster() override { return this; }
    player* as_player() override { return nullptr; }
    const monster* as_monster() const override { return this; }
    const player* as_player() const override { return nullptr; }

    // Hacks, with a capital H.
    void check_speed();

    string describe_enchantments() const;

    int action_energy(energy_use_type et) const;

    bool do_shaft() override;
    bool has_spell_of_type(spschool discipline) const;

    void bind_melee_flags();
    void calc_speed();
    bool attempt_escape() override;
    void struggle_against_net();
    void catch_breath();
    bool has_usable_tentacle() const override;

    bool is_child_tentacle() const;
    bool is_child_tentacle_of(const monster* mons) const;
    bool is_child_monster() const;
    bool is_parent_monster_of(const monster* mons) const;
    bool is_child_tentacle_segment() const;

    bool is_illusion() const;
    bool is_divine_companion() const;
    bool is_dragonkind() const override;
    int  dragon_level() const override;
    bool is_jumpy() const;

    int  spell_hd(spell_type spell = SPELL_NO_SPELL) const;
    void remove_summons(bool check_attitude = false);

    bool clear_far_engulf(bool force = false, bool /*moved*/ = false) override;
    bool search_slots(function<bool (const mon_spell_slot &)> func) const;

    bool has_facet(int facet) const;
    bool angered_by_attacks() const;

    bool is_band_follower_of(const monster& leader) const;
    bool is_band_leader_of(const monster& follower) const;
    monster* get_band_leader() const;
    void set_band_leader(const monster& leader);

private:
    int hit_dice;

private:
    bool pickup(item_def &item, mon_inv_type slot, bool msg);
    void pickup_message(const item_def &item);
    bool pickup_wand(item_def &item, bool msg, bool force = false);
    bool pickup_scroll(item_def &item, bool msg);
    bool pickup_potion(item_def &item, bool msg, bool force = false);
    bool pickup_gold(item_def &item, bool msg);
    bool pickup_launcher(item_def &launcher, bool msg, bool force = false);
    bool pickup_melee_weapon(item_def &item, bool msg);
    bool pickup_weapon(item_def &item, bool msg, bool force);
    bool pickup_armour(item_def &item, bool msg, bool force);
    bool pickup_jewellery(item_def &item, bool msg, bool force);
    bool pickup_misc(item_def &item, bool msg, bool force);
    bool pickup_missile(item_def &item, bool msg, bool force);

    void equip_message(item_def &item);
    void equip_weapon_message(item_def &item);
    void equip_armour_message(item_def &item);
    void equip_jewellery_message(item_def &item);
    void unequip_weapon(item_def &item, bool msg);
    void unequip_armour(item_def &item, bool msg);
    void unequip_jewellery(item_def &item, bool msg);

    void init_with(const monster& mons);

    int armour_bonus(const item_def &item) const;

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
    bool search_spells(function<bool (spell_type)> func) const;
    bool is_cloud_safe(const coord_def &place) const;

    void add_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void remove_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void apply_enchantment(const mon_enchant &me);
};
