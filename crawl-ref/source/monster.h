#pragma once

#include <functional>

#include "actor.h"
#include "beh-type.h"
#include "enchant-type.h"
#include "mon-ench.h"
#include "montravel-target-type.h"
#include "potion-type.h"
#include "seen-context-type.h"
#include "spl-util.h"

const int KRAKEN_TENTACLE_RANGE = 3;
const int DEFAULT_TRACKING_AMNESTY = 6; // defaults to the distance between a monster at edge of los and player.
const int MAX_BEZOT_LEVEL = 3;
const int MIN_BEZOT_LEVEL = 0;
const int BEZOT_TRACKING_CONSTANT = 2000;

#define TIDE_CALL_TURN "tide-call-turn"

#define MAX_DAMAGE_COUNTER 10000
#define ZOMBIE_BASE_AC_KEY "zombie_base_ac"
#define ZOMBIE_BASE_EV_KEY "zombie_base_ev"
#define MON_SPEED_KEY "speed"
#define CUSTOM_SPELLS_KEY "custom_spells"
#define SEEN_SPELLS_KEY "seen_spells"
#define KNOWN_MAX_HP_KEY "known_max_hp"
#define VAULT_HD_KEY "vault_hd"
#define BEZOTTED_KEY "bezotted"

#define FAKE_BLINK_KEY "fake_blink"
#define CEREBOV_DISARMED_KEY "cerebov_disarmed"

/// has a given hound already used up its howl?
#define DOOM_HOUND_HOWLED_KEY "doom_hound_howled"

#define DROPPER_MID_KEY "dropper_mid"

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

    unsigned int experience;
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
                                       // AI_SEE_MONSTER

    int damage_friendly;               // Damage taken, x2 you, x1 pets, x0 else.
    int damage_total;

    uint32_t client_id;                // for ID of monster_info between turns
    static uint32_t last_client_id;

    bool went_unseen_this_turn;
    coord_def unseen_pos;

    int turns_spent_tracking_player;   // Used to decide when to upgrade monsters.
    int tracking_amnesty;              // When this is >0, take turns off here instead of the above

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

    void init_experience();

    void mark_summoned(int longevity, bool mark_items_summoned,
                       int summon_type = 0, bool abj = true);
    bool is_summoned(int* duration = nullptr, int* summon_type = nullptr) const
        override;
    bool is_perm_summoned() const override;
    bool has_action_energy() const;
    void check_redraw(const coord_def &oldpos, bool clear_tiles = true) const;
    void apply_location_effects(const coord_def &oldpos,
                                killer_type killer = KILL_NONE,
                                int killernum = -1) override;
    void self_destruct() override;

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
    bool lose_ench_levels(const mon_enchant &e, int lev, bool infinite = false);
    void lose_energy(energy_use_type et, int div = 1, int mult = 1) override;
    void gain_energy(energy_use_type et, int div = 1, int mult = 1) override;

    void scale_hp(int num, int den);
    bool gain_exp(int exp, int max_levels_to_gain = 2);

    void react_to_damage(const actor *oppressor, int damage, beam_type flavour);
    void maybe_degrade_bone_armour();

    void add_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void remove_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void apply_enchantments();
    void apply_enchantment(const mon_enchant &me);

    bool can_drink() const;
    bool can_drink_potion(potion_type ptype) const;
    bool should_drink_potion(potion_type ptype) const;
    bool drink_potion_effect(potion_type pot_eff, bool card = false);

    bool can_evoke_jewellery(jewellery_type jtype) const;
    bool should_evoke_jewellery(jewellery_type jtype) const;
    bool evoke_jewellery_effect(jewellery_type jtype);

    void timeout_enchantments(int levels);

    bool is_travelling() const;
    bool is_patrolling() const;
    bool needs_abyss_transit() const;
    void set_transit(const level_id &destination);
    bool is_trap_safe(const coord_def& where, bool just_check = false) const;
    bool is_location_safe(const coord_def &place);
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
    void uglything_mutate(colour_t force_colour = COLOUR_UNDEF);
    void destroy_inventory();
    void load_ghost_spells();

    actor *get_foe() const;

    // actor interface
    int mindex() const override;
    int      get_hit_dice() const override;
    int      get_experience_level() const override;
    god_type deity() const override;
    bool     alive() const override;
    bool     defined() const { return alive(); }
    bool     swimming() const override;
    bool     wants_submerge() const;

    bool     submerged() const override;
    bool     can_drown() const;
    bool     floundering_at(const coord_def p) const;
    bool     floundering() const override;
    bool     extra_balanced_at(const coord_def p) const;
    bool     extra_balanced() const override;
    bool     can_pass_through_feat(dungeon_feature_type grid) const override;
    bool     is_habitable_feat(dungeon_feature_type actual_grid) const override;
    bool     shove(const char* name = "") override;

    size_type   body_size(size_part_type psize = PSIZE_TORSO,
                          bool base = false) const override;
    brand_type  damage_brand(int which_attack = -1) override;
    int         damage_type(int which_attack = -1) override;
    random_var  attack_delay(const item_def *projectile = nullptr,
                             bool rescale = true) const override;
    int         has_claws(bool allow_tran = true) const override;

    int wearing(equipment_type slot, int type, bool calc_unid = true) const
        override;
    int wearing_ego(equipment_type slot, int type, bool calc_unid = true) const
        override;
    int scan_artefacts(artefact_prop_type which_property,
                       bool calc_unid = true,
                       vector<item_def> *_unused_matches = nullptr) const
        override;

    item_def *slot_item(equipment_type eq, bool include_melded=false) const
        override;
    item_def *mslot_item(mon_inv_type sl) const;
    item_def *weapon(int which_attack = -1) const override;
    item_def *launcher() const;
    item_def *melee_weapon() const;
    item_def *missiles() const;
    item_def *shield() const override;

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

    void      wield_melee_weapon(maybe_bool msg = MB_MAYBE);
    void      swap_weapons(maybe_bool msg = MB_MAYBE);
    bool      pickup_item(item_def &item, bool msg, bool force);
    bool      drop_item(mon_inv_type eslot, bool msg);
    bool      unequip(item_def &item, bool msg, bool force = false);
    void      steal_item_from_player();
    item_def* take_item(int steal_what, mon_inv_type mslot);
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
    string conj_verb(const string &verb) const override;
    string hand_name(bool plural, bool *can_plural = nullptr) const override;
    string foot_name(bool plural, bool *can_plural = nullptr) const override;
    string arm_name(bool plural, bool *can_plural = nullptr) const override;

    bool fumbles_attack() override;

    int  skill(skill_type skill, int scale = 1,
               bool real = false, bool drained = true) const override;

    void attacking(actor *other, bool ranged) override;
    bool can_go_frenzy() const;
    bool can_go_berserk() const override;
    bool go_berserk(bool intentional, bool potion = false) override;
    bool go_frenzy(actor *source);
    bool berserk() const override;
    bool berserk_or_insane() const;
    bool can_mutate() const override;
    bool can_safely_mutate(bool temp = true) const override;
    bool can_polymorph() const override;
    bool can_bleed(bool allow_tran = true) const override;
    bool is_stationary() const override;
    bool malmutate(const string &/*reason*/) override;
    void corrupt();
    bool polymorph(int pow) override;
    void banish(actor *agent, const string &who = "", const int power = 0,
                bool force = false) override;
    void expose_to_element(beam_type element, int strength = 0,
                           bool slow_cold_blood = true) override;

    monster_type mons_species(bool zombie_base = false) const override;

    mon_holy_type holiness(bool /*temp*/ = true) const override;
    bool undead_or_demonic() const override;
    bool is_holy(bool check_spells = true) const override;
    bool is_nonliving(bool /*temp*/ = true) const override;
    int how_unclean(bool check_god = true) const;
    int known_chaos(bool check_spells_god = false) const;
    int how_chaotic(bool check_spells_god = false) const override;
    bool is_unbreathing() const override;
    bool is_insubstantial() const override;
    bool res_damnation() const override;
    int res_fire() const override;
    int res_steam() const override;
    int res_cold() const override;
    int res_elec() const override;
    int res_poison(bool temp = true) const override;
    int res_rotting(bool /*temp*/ = true) const override;
    int res_water_drowning() const override;
    bool res_sticky_flame() const override;
    int res_holy_energy() const override;
    int res_negative_energy(bool intrinsic_only = false) const override;
    bool res_torment() const override;
    int res_acid(bool calc_unid = true) const override;
    bool res_wind() const override;
    bool res_petrify(bool /*temp*/ = true) const override;
    int res_constrict() const override;
    int res_magic(bool calc_unid = true) const override;
    bool no_tele(bool calc_unid = true, bool permit_id = true,
                 bool blink = false) const override;
    bool res_corr(bool calc_unid = true, bool items = true) const override;
    bool antimagic_susceptible() const override;

    bool stasis() const override;
    bool cloud_immune(bool calc_unid = true, bool items = true) const override;

    bool airborne() const override;
    bool can_cling_to_walls() const override;
    bool is_banished() const override;
    bool is_web_immune() const override;
    bool invisible() const override;
    bool can_see_invisible(bool calc_unid = true) const override;
    bool visible_to(const actor *looker) const override;
    bool near_foe() const;
    reach_type reach_range() const override;
    bool nightvision() const override;

    bool is_icy() const override;
    bool is_fiery() const override;
    bool is_skeletal() const override;
    bool is_spiny() const;
    bool paralysed() const override;
    bool cannot_move() const override;
    bool cannot_act() const override;
    bool confused() const override;
    bool confused_by_you() const;
    bool caught() const override;
    bool asleep() const override;
    bool backlit(bool self_halo = true) const override;
    bool umbra() const override;
    int halo_radius() const override;
    int silence_radius() const override;
    int liquefying_radius() const override;
    int umbra_radius() const override;
    bool petrified() const override;
    bool petrifying() const override;
    bool liquefied_ground() const override;
    int natural_regen_rate() const;
    int off_level_regen_rate() const;

    bool friendly() const;
    bool neutral() const;
    bool good_neutral() const;
    bool strict_neutral() const;
    bool wont_attack() const override;
    bool pacified() const;

    bool has_spells() const;
    bool has_spell(spell_type spell) const override;
    mon_spell_slot_flags spell_slot_flags(spell_type spell) const;
    bool has_unclean_spell() const;
    bool has_chaotic_spell() const;
    bool has_corpse_violating_spell() const;

    bool has_attack_flavour(int flavour) const;
    bool has_damage_type(int dam_type);
    int constriction_damage() const override;

    bool can_throw_large_rocks() const override;
    bool can_speak();
    bool is_silenced() const;

    int base_armour_class() const;
    int armour_class(bool calc_unid = true) const override;
    int gdr_perc() const override { return 0; }
    int base_evasion() const;
    int evasion(ev_ignore_type evit = EV_IGNORE_NONE,
                const actor* /*attacker*/ = nullptr) const override;

    bool poison(actor *agent, int amount = 1, bool force = false) override;
    bool sicken(int strength) override;
    void paralyse(actor *, int str, string source = "") override;
    void petrify(actor *, bool force = false) override;
    bool fully_petrify(actor *foe, bool quiet = false) override;
    void slow_down(actor *, int str) override;
    void confuse(actor *, int strength) override;
    bool drain_exp(actor *, bool quiet = false, int pow = 3) override;
    bool rot(actor *, int amount, bool quiet = false, bool no_cleanup = false)
        override;
    void splash_with_acid(const actor* evildoer, int /*acid_strength*/ = -1,
                          bool /*allow_corrosion*/ = true,
                          const char* /*hurt_msg*/ = nullptr) override;
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
    void weaken(actor *attacker, int pow) override;
    void check_awaken(int disturbance) override;
    int beam_resists(bolt &beam, int hurted, bool doEffects, string source = "")
        override;

    int stat_hp() const override    { return hit_points; }
    int stat_maxhp() const override { return max_hit_points; }
    int stealth() const override { return 0; }


    bool    shielded() const override;
    int     shield_bonus() const override;
    int     shield_block_penalty() const override;
    void    shield_block_succeeded(actor *foe) override;
    int     shield_bypass_ability(int tohit) const override;
    int     missile_deflection() const override;
    void    ablate_deflection() override;

    // Combat-related class methods
    int     unadjusted_body_armour_penalty() const override { return 0; }
    int     adjusted_body_armour_penalty(int) const override { return 0; }
    int     adjusted_shield_penalty(int) const override { return 0; }
    int     armour_tohit_penalty(bool, int) const override { return 0; }
    int     shield_tohit_penalty(bool, int) const override { return 0; }

    bool is_player() const override { return false; }
    monster* as_monster() override { return this; }
    player* as_player() override { return nullptr; }
    const monster* as_monster() const override { return this; }
    const player* as_player() const override { return nullptr; }

    // Hacks, with a capital H.
    void check_speed();
    void upgrade_type(monster_type after, bool adjust_hd, bool adjust_hp);

    string describe_enchantments() const;

    int action_energy(energy_use_type et) const;

    bool do_shaft() override;
    bool has_spell_of_type(spschool_flag_type discipline) const;

    void bind_melee_flags();
    void bind_spell_flags();
    void calc_speed();
    bool attempt_escape(int attempts = 1);
    void struggle_against_net();
    bool has_usable_tentacle() const override;

    bool check_clarity(bool silent) const;

    bool is_child_tentacle() const;
    bool is_child_tentacle_of(const monster* mons) const;
    bool is_child_monster() const;
    bool is_parent_monster_of(const monster* mons) const;
    bool is_child_tentacle_segment() const;

    bool is_illusion() const;
    bool is_divine_companion() const;
    // Jumping spiders (jump instead of blink)
    bool is_jumpy() const;

    int  spell_hd(spell_type spell = SPELL_NO_SPELL) const;
    void align_avatars(bool force_friendly = false);
    void remove_avatars();
    void note_spell_cast(spell_type spell);

    bool clear_far_engulf() override;
    bool search_slots(function<bool (const mon_spell_slot &)> func) const;

    bool has_facet(int facet) const;
    bool angered_by_attacks() const;

    int bezot(int i, bool is_percentage_increase) const;
    void track_player();
    void bezot_monster();

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

    void equip(item_def &item, bool msg);
    void equip_weapon(item_def &item, bool msg);
    void equip_armour(item_def &item, bool msg);
    void equip_jewellery(item_def &item, bool msg);
    void unequip_weapon(item_def &item, bool msg);
    void unequip_armour(item_def &item, bool msg);
    void unequip_jewellery(item_def &item, bool msg);

    void init_with(const monster& mons);

    bool level_up();
    bool level_up_change();
    int armour_bonus(const item_def &item, bool calc_unid = true) const;

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
};
