/**
 * @file
 * @brief Player related functions.
**/


#ifndef PLAYER_H
#define PLAYER_H

#include "actor.h"
#include "beam.h"
#include "bitary.h"
#include "quiver.h"
#include "place-info.h"
#include "religion-enum.h"
#include "species.h"

#include <list>
#include <vector>

#ifdef USE_TILE
#include "tiledoll.h"
#endif

class targetter;

int check_stealth(void);

typedef FixedVector<int, NUM_DURATIONS> durations_t;
class player : public actor
{
public:
  // ---------------
  // Permanent data:
  // ---------------
  string your_name;
  species_type species;
  string species_name;
  job_type char_class;
  string class_name;

  // This field is here even in non-WIZARD compiles, since the
  // player might have been playing previously under wiz mode.
  bool          wizard;               // true if player has entered wiz mode.
  time_t        birth_time;           // start time of game


  // ----------------
  // Long-term state:
  // ----------------
  int elapsed_time;        // total amount of elapsed time in the game
  int elapsed_time_at_last_input; // used for elapsed_time delta display

  int hp;
  int hp_max;
  int hp_max_temp;            // temporary max HP loss (rotting)
  int hp_max_perm;            // base HPs from background (and permanent loss)

  int magic_points;
  int max_magic_points;
  int mp_max_temp;            // temporary max MP loss? (currently unused)
  int mp_max_perm;            // base MPs from background (and permanent loss)

  FixedVector<int8_t, NUM_STATS> stat_loss;
  FixedVector<int8_t, NUM_STATS> base_stats;
  FixedVector<uint8_t, NUM_STATS> stat_zero;

  int hunger;
  int disease;
  hunger_state_t hunger_state;
  uint8_t max_level;
  uint8_t hit_points_regeneration;
  uint8_t magic_points_regeneration;
  unsigned int experience;
  unsigned int total_experience; // Unaffected by draining. Used for skill cost.
  int experience_level;
  int gold;
  int zigs_completed, zig_max;

  FixedVector<int8_t, NUM_EQUIP> equip;
  FixedBitVector<NUM_EQUIP> melded;
  unsigned short unrand_reacts;

  FixedArray<int, NUM_OBJECT_CLASSES, MAX_SUBTYPES> force_autopickup;

  // PC's symbol (usually @) and colour.
  monster_type symbol;
  transformation_type form;

  FixedVector< item_def, ENDOFPACK > inv;
  FixedBitVector<NUM_RUNE_TYPES> runes;
  int obtainable_runes; // can be != 15 in Sprint

  int burden;
  burden_state_type burden_state;
  FixedVector<spell_type, MAX_KNOWN_SPELLS> spells;
  set<spell_type> old_vehumet_gifts;

  uint8_t spell_no;
  set<spell_type> vehumet_gifts;
  game_direction_type char_direction;
  bool opened_zot;
  bool royal_jelly_dead;
  bool transform_uncancellable;
  bool fishtail; // Merfolk fishtail transformation

  unsigned short pet_target;

  durations_t duration;
  int rotting;
  int berserk_penalty;                // penalty for moving while berserk

  FixedVector<int, NUM_ATTRIBUTES> attribute;
  FixedVector<uint8_t, NUM_AMMO> quiver; // default items for quiver
  FixedVector<int, NUM_OBJECT_CLASSES> sacrifice_value;

  undead_state_type is_undead;

  int  friendly_pickup;       // pickup setting for allies
  bool dead; // ... but pending revival
  int lives;
  int deaths;
  float temperature; // For lava orcs.
  float temperature_last;

  FixedVector<uint8_t, NUM_SKILLS>  skills; //!< skill level
  FixedVector<int8_t, NUM_SKILLS>  train; //!< 0: disabled, 1: normal, 2: focus.
  FixedVector<int8_t, NUM_SKILLS>  train_alt; //<! config of the other mode.
  FixedVector<unsigned int, NUM_SKILLS>  training; //<! percentage of XP used
  FixedBitVector<NUM_SKILLS> can_train; //!<Is training this skill allowed
  FixedVector<unsigned int, NUM_SKILLS> skill_points;
  FixedVector<unsigned int, NUM_SKILLS> ct_skill_points; //<!track skill points
                                                    //<!gained by crosstraining
  FixedVector<uint8_t, NUM_SKILLS>  skill_order;

  bool auto_training;
  list<skill_type> exercises;     //<! recent practise events
  list<skill_type> exercises_all; //<! also include events for disabled skills
  set<skill_type> stop_train; //<! need to check if we can still train
  set<skill_type> start_train; //<! we can resume training

  // Skill menu states
  skill_menu_state skill_menu_do;
  skill_menu_state skill_menu_view;

  //Ashenzari transfer knowledge
  skill_type    transfer_from_skill;
  skill_type    transfer_to_skill;
  unsigned int  transfer_skill_points;
  unsigned int  transfer_total_skill_points;

  vector<skill_type> sage_skills; // skills with active Sage
  vector<int> sage_xp;            // how much more XP to redirect
  vector<int> sage_bonus;         // how much bonus XP to give in these skills

  int  skill_cost_level;
  int  exp_available;
  int  zot_points; // ZotDef currency

  int exp_docked, exp_docked_total; // Ashenzari's wrath

  FixedArray<uint8_t, 6, MAX_SUBTYPES> item_description;
  FixedVector<unique_item_status_type, MAX_UNRANDARTS> unique_items;
  FixedBitVector<NUM_MONSTERS> unique_creatures;

  // NOTE: The kills member is a pointer to a KillMaster object,
  // rather than the object itself, so that we can get away with
  // just a forward declare of the KillMaster class, rather than
  // having to #include kills.h and thus make every single .cc file
  // dependent on kills.h.  Having a pointer means that we have
  // to do our own implementations of copying the player object,
  // since the default implementations will lead to the kills member
  // pointing to freed memory, or worse yet lead to the same piece of
  // memory being freed twice.
  KillMaster* kills;

  branch_type where_are_you;
  int depth;

  FixedVector<uint8_t, 30> branch_stairs;

  god_type religion;
  string god_name;
  string jiyva_second_name;       // Random second name of Jiyva
  uint8_t piety;
  uint8_t piety_hysteresis;       // amount of stored-up docking
  uint8_t gift_timeout;
  FixedVector<uint8_t, NUM_GODS>  penance;
  FixedVector<uint8_t, NUM_GODS>  worshipped;
  FixedVector<short,   NUM_GODS>  num_current_gifts;
  FixedVector<short,   NUM_GODS>  num_total_gifts;
  FixedBitVector<   NUM_GODS>  one_time_ability_used;
  FixedVector<uint8_t, NUM_GODS>  piety_max;

  // Nemelex sacrifice toggles
  FixedBitVector<NUM_NEMELEX_GIFT_TYPES> nemelex_sacrificing;

  FixedVector<uint8_t, NUM_MUTATIONS> mutation;
  FixedVector<uint8_t, NUM_MUTATIONS> innate_mutations;
  FixedVector<uint8_t, NUM_MUTATIONS> temp_mutations;

  struct demon_trait
  {
      int           level_gained;
      mutation_type mutation;
  };

  vector<demon_trait> demonic_traits;

  int magic_contamination;

  FixedBitVector<NUM_FIXED_BOOKS> had_book;
  FixedBitVector<NUM_SPELLS>      seen_spell;
  FixedVector<uint32_t, NUM_WEAPONS> seen_weapon;
  FixedVector<uint32_t, NUM_ARMOURS> seen_armour;
  FixedBitVector<NUM_MISCELLANY>     seen_misc;
  uint8_t                            octopus_king_rings;

  uint8_t normal_vision;        // how far the species gets to see
  uint8_t current_vision;       // current sight radius (cells)

  int           real_time;            // real time played (in seconds)
  int           num_turns;            // number of turns taken
  int           exploration;          // levels explored (16.16 bit real number)

  int           last_view_update;     // what turn was the view last updated?

  // Warning: these two are quite different.
  //
  // The spell table is an index to a specific spell slot (you.spells).
  // The ability table lists the ability (ABIL_*) which prefers that letter.
  //
  // In other words, the spell table contains hard links and the ability
  // table contains soft links.
  FixedVector<int, 52>           spell_letter_table;   // ref to spell by slot
  FixedVector<ability_type, 52>  ability_letter_table; // ref to abil by enum

  // Maps without allow_dup that have been already used.
  set<string> uniq_map_tags;
  set<string> uniq_map_names;
  // All maps, by level.
  map<level_id, vector<string> > vault_list;

  PlaceInfo global_info;
  player_quiver* m_quiver;

  // monsters mesmerising player; should be protected, but needs to be saved
  // and restored.
  vector<int> beholders;

  // monsters causing fear to the player; see above
  vector<int> fearmongers;

  // Delayed level actions.  This array is never trimmed, as usually D:1 won't
  // be loaded again until the very end.
  vector<daction_type> dactions;

  // Path back from portal levels.
  vector<level_pos> level_stack;

  // The player's knowledge about item types.
  id_arr type_ids;
  // Additional information, about tried unidentified items.
  // (e.g. name of item, for scrolls of RC, ID, EA)
  CrawlHashTable type_id_props;

  // The version the save was last played with.
  string prev_save_version;

  // The type of a zotdef wave, if any.
  string zotdef_wave_name;
  // The biggest assigned monster id so far.
  mid_t last_mid;

  // Count of various types of actions made.
  map<pair<caction_type, int>, FixedVector<int, 27> > action_count;

  // Which branches have been noted to have been left during this game.
  FixedBitVector<NUM_BRANCHES> branches_left;

  // For now, only control the speed of abyss morphing.
  int abyss_speed;

  // Prompts or actions the player must answer before continuing.
  // A stack -- back() is the first to go.
  vector<pair<uncancellable_type, int> > uncancel;

  // A list of allies awaiting an active recall
  vector<mid_t> recall_list;

  // Hash seeds for deterministic stuff.
  FixedVector<uint32_t, NUM_SEEDS> game_seeds;

  // -------------------
  // Non-saved UI state:
  // -------------------
  unsigned short prev_targ;
  coord_def      prev_grd_targ;
  coord_def      prev_move;

  // Coordinates of last travel target; note that this is never used by
  // travel itself, only by the level-map to remember the last travel target.
  short travel_x, travel_y;
  level_id travel_z;

  runrest running;                    // Nonzero if running/traveling.
  bool travel_ally_pace;

  bool received_weapon_warning;
  bool received_noskill_warning;

  delay_queue_type delay_queue;       // pending actions

  time_t last_keypress_time;
  bool xray_vision;
  int8_t bondage_level;  // how much an Ash worshipper is into bondage
  int8_t bondage[NUM_ET];
  map<skill_type, int8_t> skill_boost; // Skill bonuses.

  // The last spell cast by the player.
  spell_type last_cast_spell;


  // ---------------------------
  // Volatile (same-turn) state:
  // ---------------------------
  bool turn_is_over; // flag signaling that player has performed a timed action

  // If true, player is headed to the Abyss.
  bool banished;
  string banished_by;

  bool wield_change;          // redraw weapon
  bool redraw_quiver;         // redraw quiver
  uint64_t redraw_status_flags;

  bool redraw_title;
  bool redraw_hit_points;
  bool redraw_magic_points;
  bool redraw_temperature;
  FixedVector<bool, NUM_STATS> redraw_stats;
  bool redraw_experience;
  bool redraw_armour_class;
  bool redraw_evasion;

  colour_t flash_colour;
  targetter *flash_where;

  int time_taken;

  int old_hunger;            // used for hunger delta-meter (see output.cc)

  // Set when the character is going to a new level, to guard against levgen
  // failures
  dungeon_feature_type transit_stair;
  bool entering_level;

  int    escaped_death_cause;
  string escaped_death_aux;

  int turn_damage;   // cumulative damage per turn
  int damage_source; // death source of last damage done to player
  int source_damage; // cumulative damage for you.damage_source

  // When other levels are loaded (e.g. viewing), is the player on this level?
  bool on_current_level;

  // Did you spent this turn walking (/flying)?
  // 0 = no, 1 = cardinal move, 2 = diagonal move
  int walking;

  // View code clears and needs new data in places where we can't announce the
  // portal right away; delay the announcements then.
  int seen_portals;
  // Same with invisible monsters, for ring auto-id.
  bool seen_invis;

  // Number of viewport refreshes.
  unsigned int frame_no;


  // ---------------------
  // The save file itself.
  // ---------------------
  package *save;

protected:
    FixedVector<PlaceInfo, NUM_BRANCHES> branch_info;

public:
    player();
    player(const player &other);
    ~player();

    void copy_from(const player &other);

    void init();
    void init_skills();

    // Set player position without updating view geometry.
    void set_position(const coord_def &c);
    // Low-level move the player. Use this instead of changing pos directly.
    void moveto(const coord_def &c, bool clear_net = true);
    bool move_to_pos(const coord_def &c, bool clear_net = true);
    // Move the player during an abyss shift.
    void shiftto(const coord_def &c);
    bool blink_to(const coord_def& c, bool quiet = false);

    void reset_prev_move();

    int stat(stat_type stat, bool nonneg = true) const;
    int strength(bool nonneg = true) const;
    int intel(bool nonneg = true) const;
    int dex(bool nonneg = true) const;
    int max_stat(stat_type stat) const;
    int max_strength() const;
    int max_intel() const;
    int max_dex() const;

    bool in_water() const;
    bool in_lava() const;
    bool in_liquid() const;
    bool can_swim(bool permanently = false) const;
    int visible_igrd(const coord_def&) const;
    bool can_cling_to_walls() const;
    bool is_banished() const;
    bool is_web_immune() const;
    bool cannot_speak() const;
    bool invisible() const;
    bool misled() const;
    bool can_see_invisible() const;
    bool can_see_invisible(bool unid, bool items = true) const;
    bool visible_to(const actor *looker) const;
    bool can_see(const actor* a) const;
    bool nightvision() const;
    reach_type reach_range() const;
    bool see_cell(const coord_def& p) const;

    // Is c in view but behind a transparent wall?
    bool trans_wall_blocking(const coord_def &c) const;

    bool is_icy() const;
    bool is_fiery() const;
    bool is_skeletal() const;

    bool tengu_flight() const;

    // Dealing with beholders. Implemented in behold.cc.
    void add_beholder(const monster* mon, bool axe = false);
    bool beheld() const;
    bool beheld_by(const monster* mon) const;
    monster* get_beholder(const coord_def &pos) const;
    monster* get_any_beholder() const;
    void remove_beholder(const monster* mon);
    void clear_beholders();
    void beholders_check_noise(int loudness, bool axe = false);
    void update_beholders();
    void update_beholder(const monster* mon);
    bool possible_beholder(const monster* mon) const;

    // Dealing with fearmongers. Implemented in fearmonger.cc.
    bool add_fearmonger(const monster* mon);
    bool afraid() const;
    bool afraid_of(const monster* mon) const;
    monster* get_fearmonger(const coord_def &pos) const;
    monster* get_any_fearmonger() const;
    void remove_fearmonger(const monster* mon);
    void clear_fearmongers();
    void fearmongers_check_noise(int loudness, bool axe = false);
    void update_fearmongers();
    void update_fearmonger(const monster* mon);

    bool made_nervous_by(const coord_def &pos);

    kill_category kill_alignment() const;

    bool has_spell(spell_type spell) const;

    size_type transform_size(transformation_type tform,
                             int psize = PSIZE_TORSO) const;
    string shout_verb() const;

    item_def *slot_item(equipment_type eq, bool include_melded=false) const;

    map<int,int> last_pickup;

    // actor
    int mindex() const;
    int get_experience_level() const;
    actor_type atype() const { return ACT_PLAYER; }
    monster* as_monster() { return NULL; }
    player* as_player() { return this; }
    const monster* as_monster() const { return NULL; }
    const player* as_player() const { return this; }

    god_type  deity() const;
    bool      alive() const;
    bool      is_summoned(int* duration = NULL, int* summon_type = NULL) const;
    bool      is_perm_summoned() const { return false; };

    bool        swimming() const;
    bool        submerged() const;
    bool        floundering() const;
    bool        extra_balanced() const;
    bool        shove(const char* feat_name = "");
    bool        can_pass_through_feat(dungeon_feature_type grid) const;
    bool        is_habitable_feat(dungeon_feature_type actual_grid) const;
    size_type   body_size(size_part_type psize = PSIZE_TORSO, bool base = false) const;
    int         body_weight(bool base = false) const;
    int         total_weight() const;
    brand_type  damage_brand(int which_attack = -1);
    int         damage_type(int which_attack = -1);
    int         constriction_damage() const;

    int       has_claws(bool allow_tran = true) const;
    bool      has_usable_claws(bool allow_tran = true) const;
    int       has_talons(bool allow_tran = true) const;
    bool      has_usable_talons(bool allow_tran = true) const;
    int       has_fangs(bool allow_tran = true) const;
    int       has_usable_fangs(bool allow_tran = true) const;
    int       has_tail(bool allow_tran = true) const;
    int       has_usable_tail(bool allow_tran = true) const;
    bool      has_usable_offhand() const;
    int       has_pseudopods(bool allow_tran = true) const;
    int       has_usable_pseudopods(bool allow_tran = true) const;
    int       has_tentacles(bool allow_tran = true) const;
    int       has_usable_tentacles(bool allow_tran = true) const;

    int wearing(equipment_type slot, int sub_type, bool calc_unid = true) const;
    int wearing_ego(equipment_type slot, int type, bool calc_unid = true) const;
    int scan_artefacts(artefact_prop_type which_property,
                       bool calc_unid = true) const;

    item_def *weapon(int which_attack = -1) const;
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

    string name(description_level_type type, bool force_visible = false) const;
    string pronoun(pronoun_type pro, bool force_visible = false) const;
    string conj_verb(const string &verb) const;
    string hand_name(bool plural, bool *can_plural = NULL) const;
    string foot_name(bool plural, bool *can_plural = NULL) const;
    string arm_name(bool plural, bool *can_plural = NULL) const;
    string unarmed_attack_name() const;

    bool fumbles_attack(bool verbose = true);
    bool cannot_fight() const;
    bool fights_well_unarmed(int heavy_armour_penalty);

    void attacking(actor *other);
    bool can_go_berserk() const;
    bool can_go_berserk(bool intentional, bool potion = false,
                        bool quiet = false) const;
    bool can_jump() const;
    bool can_jump(bool quiet) const;
    void go_berserk(bool intentional, bool potion = false);
    bool berserk() const;
    bool has_lifeforce() const;
    bool can_mutate() const;
    bool can_safely_mutate() const;
    bool is_lifeless_undead() const;
    bool can_polymorph() const;
    bool can_bleed(bool allow_tran = true) const;
    bool is_stationary() const;
    bool malmutate(const string &reason);
    bool polymorph(int pow);
    void backlight();
    void banish(actor *agent, const string &who = "");
    void blink(bool allow_partial_control = true);
    void teleport(bool right_now = false,
                  bool abyss_shift = false,
                  bool wizard_tele = false);
    void drain_stat(stat_type stat, int amount, actor* attacker);

    void expose_to_element(beam_type element, int strength = 0,
                           bool damage_inventory = true,
                           bool slow_cold_blood = true);
    void god_conduct(conduct_type thing_done, int level);

    void make_hungry(int nutrition, bool silent = true);
    bool poison(actor *agent, int amount = 1, bool force = false);
    bool sicken(int amount, bool allow_hint = true, bool quiet = false);
    void paralyse(actor *, int str, string source = "");
    void petrify(actor *, bool force = false);
    bool fully_petrify(actor *foe, bool quiet = false);
    void slow_down(actor *, int str);
    void confuse(actor *, int strength);
    void weaken(actor *attacker, int pow);
    bool heal(int amount, bool max_too = false);
    bool drain_exp(actor *, bool quiet = false, int pow = 3);
    bool rot(actor *, int amount, int immediate = 0, bool quiet = false);
    void sentinel_mark(bool trap = false);
    int hurt(const actor *attacker, int amount,
             beam_type flavour = BEAM_MISSILE,
             bool cleanup_dead = true,
             bool attacker_effects = true);

    bool wont_attack() const { return true; };
    mon_attitude_type temp_attitude() const { return ATT_FRIENDLY; };

    monster_type mons_species(bool zombie_base = false) const;

    mon_holy_type holiness() const;
    bool undead_or_demonic() const;
    bool is_holy(bool spells = true) const;
    bool is_unholy(bool spells = true) const;
    bool is_evil(bool spells = true) const;
    bool is_chaotic() const;
    bool is_artificial() const;
    bool is_unbreathing() const;
    bool is_insubstantial() const;
    bool is_cloud_immune(cloud_type) const;
    int res_acid(bool calc_unid = true) const;
    int res_fire() const;
    int res_holy_fire() const;
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
    int res_wind() const;
    int res_petrify(bool temp = true) const;
    int res_constrict() const;
    int res_magic() const;
    bool no_tele(bool calc_unid = true, bool permit_id = true,
                 bool blink = false) const;

    bool gourmand(bool calc_unid = true, bool items = true) const;
    bool res_corr(bool calc_unid = true, bool items = true) const;
    bool clarity(bool calc_unid = true, bool items = true) const;
    bool stasis(bool calc_unid = true, bool items = true) const;

    flight_type flight_mode() const;
    bool cancellable_flight() const;
    bool permanent_flight() const;
    bool racial_permanent_flight() const;

    bool paralysed() const;
    bool cannot_move() const;
    bool cannot_act() const;
    bool confused() const;
    bool caught() const;
    bool backlit(bool check_haloed = true, bool self_halo = true) const;
    bool umbra(bool check_haloed = true, bool self_halo = true) const;
    int halo_radius2() const;
    int silence_radius2() const;
    int liquefying_radius2() const;
    int umbra_radius2() const;
    int heat_radius2() const;
    bool glows_naturally() const;
    bool petrifying() const;
    bool petrified() const;
    bool liquefied_ground() const;
    bool incapacitated() const
    {
        return actor::incapacitated() || stat_zero[STAT_DEX];
    }

    bool asleep() const;
    void hibernate(int power = 0);
    void put_to_sleep(actor *, int power = 0);
    void awake();
    void check_awaken(int disturbance);
    int beam_resists(bolt &beam, int hurted, bool doEffects, string source);

    bool can_throw_large_rocks() const;
    bool can_smell() const;

    int armour_class() const;
    int gdr_perc() const;
    int melee_evasion(const actor *attacker,
                      ev_ignore_type evit = EV_IGNORE_NONE) const;

    int stat_hp() const     { return hp; }
    int stat_maxhp() const  { return hp_max; }
    int stealth() const     { return check_stealth(); }

    int shield_bonus() const;
    int shield_block_penalty() const;
    int shield_bypass_ability(int tohit) const;
    void shield_block_succeeded(actor *foe);
    int missile_deflection() const;

    // Combat-related adjusted penalty calculation methods
    int unadjusted_body_armour_penalty() const;
    int adjusted_body_armour_penalty(int scale = 1,
                                     bool use_size = false) const;
    int adjusted_shield_penalty(int scale = 1) const;
    int armour_tohit_penalty(bool random_factor, int scale = 1) const;
    int shield_tohit_penalty(bool random_factor, int scale = 1) const;

    bool wearing_light_armour(bool with_skill = false) const;
    int  skill(skill_type skill, int scale =1, bool real = false) const;

    bool do_shaft();

    bool can_do_shaft_ability(bool quiet = false) const;
    bool do_shaft_ability();

    void apply_location_effects(const coord_def &oldpos,
                                killer_type killer = KILL_NONE,
                                int killernum = -1);

    ////////////////////////////////////////////////////////////////

    PlaceInfo& get_place_info() const ; // Current place info
    PlaceInfo& get_place_info(branch_type branch) const;
    void clear_place_info();

    void goto_place(const level_id &level);

    void set_place_info(PlaceInfo info);
    // Returns copies of the PlaceInfo; modifying the vector won't
    // modify the player object.
    vector<PlaceInfo> get_all_place_info(bool visited_only = false,
                                         bool dungeon_only = false) const;

    bool did_escape_death() const;
    void reset_escaped_death();

    void add_gold(int delta);
    void del_gold(int delta);
    void set_gold(int amount);

    void increase_duration(duration_type dur, int turns, int cap = 0,
                           const char* msg = NULL);
    void set_duration(duration_type dur, int turns, int cap = 0,
                      const char *msg = NULL);

    bool attempt_escape(int attempts = 1);
    int usable_tentacles() const;
    bool has_usable_tentacle() const;

protected:
    void _removed_beholder();
    bool _possible_beholder(const monster* mon) const;

    void _removed_fearmonger();
    bool _possible_fearmonger(const monster* mon) const;

};

#ifdef DEBUG_GLOBALS
#define you (*real_you)
#endif
extern player you;

struct player_save_info
{
    string name;
    unsigned int experience;
    int experience_level;
    bool wizard;
    species_type species;
    string species_name;
    string class_name;
    god_type religion;
    string god_name;
    string jiyva_second_name;
    game_type saved_game_type;

#ifdef USE_TILE
    dolls_data doll;
#endif

    bool save_loadable;
    string filename;

    player_save_info& operator=(const player& rhs);
    bool operator<(const player_save_info& rhs) const;
    string short_desc() const;
};

class monster;
struct item_def;

// Helper. Use move_player_to_grid or player::apply_location_effects instead.
void moveto_location_effects(dungeon_feature_type old_feat,
                             bool stepped=false, bool allow_shift=true,
                             const coord_def& old_pos=coord_def());

bool check_moveto(const coord_def& p, const string &move_verb = "step",
                  const string &msg = "");
bool check_moveto_terrain(const coord_def& p, const string &move_verb,
                          const string &msg = "", bool *prompted = nullptr);
bool check_moveto_cloud(const coord_def& p, const string &move_verb = "step",
                        bool *prompted = nullptr);
bool check_moveto_exclusion(const coord_def& p,
                            const string &move_verb = "step",
                            bool *prompted = nullptr);
bool check_moveto_trap(const coord_def& p, const string &move_verb = "step",
        bool *prompted = nullptr);

void move_player_to_grid(const coord_def& p, bool stepped, bool allow_shift);

bool is_map_persistent(void);
bool player_in_mappable_area(void);
bool player_in_connected_branch(void);
bool player_in_hell(void);

static inline bool player_in_branch(int branch)
{
    return you.where_are_you == branch;
};

bool berserk_check_wielded_weapon(void);
bool player_equip_unrand(int unrand_index);
bool player_can_hit_monster(const monster* mon);
bool player_can_hear(const coord_def& p, int hear_distance = 999);

bool player_is_shapechanged(void);

bool is_effectively_light_armour(const item_def *item);
bool player_effectively_in_light_armour();

static inline int player_under_penance(god_type god = you.religion)
{
    return you.penance[god];
}

int burden_change(void);

int carrying_capacity(burden_state_type bs = BS_OVERLOADED);

int player_energy(void);

int player_raw_body_armour_evasion_penalty();
int player_adjusted_shield_evasion_penalty(int scale);
int player_armour_shield_spell_penalty();
int player_evasion(ev_ignore_type evit = EV_IGNORE_NONE);

int player_movement_speed(bool ignore_burden = false);

int player_hunger_rate(bool temp = true);

int calc_hunger(int food_cost);

int player_icemail_armour_class();

bool player_stoneskin();

int player_mag_abil(bool is_weighted);

int player_prot_life(bool calc_unid = true, bool temp = true,
                     bool items = true);

int player_regen(void);

int player_res_cold(bool calc_unid = true, bool temp = true,
                    bool items = true);
int player_res_acid(bool calc_unid = true, bool items = true);
int player_acid_resist_factor();

int player_res_torment(bool calc_unid = true, bool temp = true);
int player_kiku_res_torment();

int player_likes_chunks(bool permanently = false);
bool player_likes_water(bool permanently = false);
bool player_likes_lava(bool permanently = false);

int player_mutation_level(mutation_type mut, bool temp = true);

int player_res_electricity(bool calc_unid = true, bool temp = true,
                           bool items = true);

int player_res_fire(bool calc_unid = true, bool temp = true,
                    bool items = true);
int player_res_sticky_flame(bool calc_unid = true, bool temp = true,
                            bool items = true);
int player_res_steam(bool calc_unid = true, bool temp = true,
                     bool items = true);

int player_res_poison(bool calc_unid = true, bool temp = true,
                      bool items = true);
int player_res_magic(bool calc_unid = true, bool temp = true);

bool player_control_teleport(bool temp = true);

int player_shield_class(void);


int player_spec_air(void);
int player_spec_cold(void);
int player_spec_conj(void);
int player_spec_death(void);
int player_spec_earth(void);
int player_spec_fire(void);
int player_spec_hex(void);
int player_spec_charm(void);
int player_spec_poison(void);
int player_spec_summ(void);

int player_speed(void);

int player_spell_levels(void);

int player_sust_abil(bool calc_unid = true);

int player_teleport(bool calc_unid = true);

bool items_give_ability(const int slot, artefact_prop_type abil);

int slaying_bonus(weapon_property_type which_affected, bool ranged = false);

unsigned int exp_needed(int lev, int exp_apt = -99);
bool will_gain_life(int lev);

int get_expiration_threshold(duration_type dur);
bool dur_expiring(duration_type dur);
void display_char_status(void);

void forget_map(bool rot = false);

int get_exp_progress();
void gain_exp(unsigned int exp_gained, unsigned int* actual_gain = NULL);

bool player_can_open_doors();

void level_change(int source = NON_MONSTER, const char *aux = NULL,
                  bool skip_attribute_increase = false);
void adjust_level(int diff, bool just_xp = false);

bool player_genus(genus_type which_genus,
                   species_type species = SP_UNKNOWN);
bool is_player_same_genus(const monster_type mon, bool = false);
monster_type player_mons(bool transform = true);
void update_player_symbol();
void update_vision_range();

bool you_can_wear(int eq, bool special_armour = false);
bool player_has_feet(bool temp = true);
bool player_wearing_slot(int eq);
bool you_tran_can_wear(const item_def &item);
bool you_tran_can_wear(int eq, bool check_mutation = false);

bool enough_hp(int minimum, bool suppress_msg, bool abort_macros = true);
bool enough_mp(int minimum, bool suppress_msg,
               bool abort_macros = true, bool include_items = true);
bool enough_zp(int minimum, bool suppress_msg);

void dec_hp(int hp_loss, bool fatal, const char *aux = NULL);
void dec_mp(int mp_loss, bool silent = false);
void drain_mp(int mp_loss);

void inc_mp(int mp_gain, bool silent = false);
void inc_hp(int hp_gain);
void flush_mp();

void rot_hp(int hp_loss);
void unrot_hp(int hp_recovered);
int player_rotted();
void rot_mp(int mp_loss);

void inc_max_hp(int hp_gain);
void dec_max_hp(int hp_loss);

void deflate_hp(int new_level, bool floor);
void set_hp(int new_amount);

int get_real_hp(bool trans, bool rotted = false);
int get_real_mp(bool include_items);

int get_contamination_level();
string describe_contamination(int level);

void set_mp(int new_amount);

void contaminate_player(int change, bool controlled = false, bool msg = true);

bool confuse_player(int amount, bool resistable = true);

bool curare_hits_player(int death_source, int amount, const bolt &beam);
bool poison_player(int amount, string source, string source_aux = "",
                   bool force = false);
void paralyse_player(string source, int amount = 0, int factor = 1);
void dec_poison_player();
void reduce_poison_player(int amount);
bool miasma_player(string source, string source_aux = "");

bool napalm_player(int amount, string source, string source_aux = "");
void dec_napalm_player(int delay);

bool slow_player(int turns);
void dec_slow_player(int delay);
void dec_exhaust_player(int delay);

bool haste_player(int turns, bool rageext = false);
void dec_haste_player(int delay);
void dec_elixir_player(int delay);
bool flight_allowed(bool quiet = false);
void fly_player(int pow, bool already_flying = false);
void float_player();
bool land_player(bool quiet = false);

void dec_disease_player(int delay);

void dec_color_smoke_trail();

void handle_player_drowning(int delay);

bool player_weapon_wielded();

// Determines if the given grid is dangerous for the player to enter.
bool is_feat_dangerous(dungeon_feature_type feat, bool permanently = false,
                       bool ignore_items = false);

void run_macro(const char *macroname = NULL);

int count_worn_ego(int which_ego);
bool need_expiration_warning(duration_type dur, coord_def p = you.pos());
bool need_expiration_warning(coord_def p = you.pos());

void count_action(caction_type type, int subtype = 0);
#endif
