/*
 *  File:       player.cc
 *  Summary:    Player related functions.
 *  Written by: Linley Henzell
 */


#ifndef PLAYER_H
#define PLAYER_H

#include "actor.h"
#include "beam.h"
#include "quiver.h"
#include "itemprop-enum.h"
#include "place-info.h"

#include "species.h"

#include <vector>
#include <stdint.h>

#ifdef USE_TILE
#include "tiledoll.h"
#endif

class player : public actor
{
public:
  bool turn_is_over; // flag signaling that player has performed a timed action

  // If true, player is headed to the Abyss.
  bool banished;
  std::string banished_by;

  int  friendly_pickup;       // pickup setting for allies

  unsigned short prev_targ;
  coord_def      prev_grd_targ;

  std::string your_name;
  species_type species;
  job_type char_class;

  // Coordinates of last travel target; note that this is never used by
  // travel itself, only by the level-map to remember the last travel target.
  short travel_x, travel_y;
  level_id travel_z;

  runrest running;            // Nonzero if running/traveling.

  unsigned short unrand_reacts;

  long elapsed_time;        // total amount of elapsed time in the game

  int disease;

  char max_level;

  coord_def prev_move;

  int hunger;
  FixedVector<signed char, NUM_EQUIP> equip;
  FixedVector<bool, NUM_EQUIP> melded;

  int hp;
  int hp_max;
  int base_hp;                // temporary max HP loss (rotting)
  int base_hp2;               // base HPs from levels (and permanent loss)

  int magic_points;
  int max_magic_points;
  int base_magic_points;      // temporary max MP loss? (currently unused)
  int base_magic_points2;     // base MPs from levels and potions of magic

  FixedVector<char, NUM_STATS> stat_loss;
  FixedVector<char, NUM_STATS> base_stats;
  FixedVector<int, NUM_STATS> stat_zero;
  stat_type last_chosen;

  char hunger_state;

  bool wield_change;          // redraw weapon
  bool redraw_quiver;         // redraw quiver
  bool received_weapon_warning;

  unsigned long redraw_status_flags;

  // PC's symbol (usually @) and colour.
  int symbol;
  int colour;

  bool redraw_hit_points;
  bool redraw_magic_points;
  FixedVector<bool, NUM_STATS> redraw_stats;
  bool redraw_experience;
  bool redraw_armour_class;
  bool redraw_evasion;

  unsigned char flash_colour;

  unsigned char hit_points_regeneration;
  unsigned char magic_points_regeneration;

  unsigned long experience;
  int experience_level;
  int gold;
  char class_name[30];
  int time_taken;

  int shield_blocks;         // number of shield blocks since last action

  FixedVector< item_def, ENDOFPACK > inv;

  int burden;
  burden_state_type burden_state;
  FixedVector<spell_type, 25> spells;
  char spell_no;
  game_direction_type char_direction;
  bool opened_zot;
  bool royal_jelly_dead;
  bool transform_uncancellable;

  unsigned short pet_target;

  int absdepth0; // offset by one (-1 == 0, 0 == 1, etc.) for display

  // durational things
  FixedVector<int, NUM_DURATIONS> duration;

  int rotting;

  int berserk_penalty;                // penalty for moving while berserk

  FixedVector<unsigned long, NUM_ATTRIBUTES> attribute;
  FixedVector<unsigned char, NUM_AMMO> quiver; // default items for quiver
  FixedVector<long, NUM_OBJECT_CLASSES> sacrifice_value;

  undead_state_type is_undead;

  delay_queue_type delay_queue;       // pending actions

  FixedVector<unsigned char, 50>  skills;
  FixedVector<bool, 50>  practise_skill;
  FixedVector<unsigned int, 50>   skill_points;
  FixedVector<unsigned char, 50>  skill_order;

  skill_type sage_bonus_skill;  // If Sage is in effect, which skill it affects.
  int sage_bonus_degree;        // How much bonus XP to give in that skill.

  int  skill_cost_level;
  int  total_skill_points;
  int  exp_available;

  FixedArray<unsigned char, 6, 50> item_description;
  FixedVector<unique_item_status_type, MAX_UNRANDARTS> unique_items;
  FixedVector<bool, NUM_MONSTERS> unique_creatures;

  // NOTE: The kills member is a pointer to a KillMaster object,
  // rather than the object itself, so that we can get away with
  // just a forward declare of the KillMaster class, rather than
  // having to #include kills.h and thus make every single .cc file
  // dependant on kills.h.  Having a pointer means that we have
  // to do our own implementations of copying the player object,
  // since the default implementations will lead to the kills member
  // pointing to freed memory, or worse yet lead to the same piece of
  // memory being freed twice.
  KillMaster* kills;

  level_area_type level_type;

  // Human-readable name for portal vault. Will be set to level_type_tag
  // if not explicitly set by the entry portal.
  std::string level_type_name;

  // Three-letter extension for portal vault bones files. Will be set
  // to first three letters of level_type_tag if not explicitly set by
  // the entry portal.
  std::string level_type_ext;

  // Abbreviation of portal vault name, for use in notes.  If not
  // explicitly set by the portal vault, will be set from level_type_name
  // or level_type_tag if either is short enough, or the shorter of the
  // two will be truncated if neither is short enough.
  std::string level_type_name_abbrev;

  // Item origin string for items from portal vaults, so that dumps
  // can have origins like "You found it in on level 2 of a ziggurat".
  // Will be set relative to level_type_name if not explicitly set.
  std::string level_type_origin;

  // .des file tag for portal vault
  std::string level_type_tag;

  entry_cause_type entry_cause;
  god_type         entry_cause_god;

  branch_type where_are_you;

  FixedVector<unsigned char, 30> branch_stairs;

  god_type religion;
  std::string second_god_name; // Random second name of Jiyva
  unsigned char piety;
  unsigned char piety_hysteresis;       // amount of stored-up docking
  unsigned char gift_timeout;
  FixedVector<unsigned char, MAX_NUM_GODS>  penance;
  FixedVector<unsigned char, MAX_NUM_GODS>  worshipped;
  FixedVector<short,         MAX_NUM_GODS>  num_gifts;


  FixedVector<unsigned char, NUM_MUTATIONS> mutation;
  FixedVector<unsigned char, NUM_MUTATIONS> demon_pow;

  struct demon_trait
  {
      int           level_gained;
      mutation_type mutation;
  };

  std::vector<demon_trait> demonic_traits;

  std::vector<item_def> powered_by_death_corpses;

  unsigned char magic_contamination;

  FixedVector<bool, NUM_FIXED_BOOKS> had_book;
  FixedVector<bool, NUM_SPELLS>      seen_spell;
  FixedVector<uint32_t, NUM_WEAPONS> seen_weapon;
  FixedVector<uint32_t, NUM_ARMOURS> seen_armour;

  unsigned char normal_vision;        // how far the species gets to see
  unsigned char current_vision;       // current sight radius (cells)

  unsigned char hell_exit;            // which level player goes to on hell exit

  // This field is here even in non-WIZARD compiles, since the
  // player might have been playing previously under wiz mode.
  bool          wizard;               // true if player has entered wiz mode.
  time_t        birth_time;           // start time of game

  time_t        start_time;           // start time of session
  long          real_time;            // real time played (in seconds)
  long          num_turns;            // number of turns taken

  long          last_view_update;     // what turn was the view last updated?

  int           old_hunger;  // used for hunger delta-meter (see output.cc)

  // Set when the character is going to a new level, to guard against levgen
  // failures
  dungeon_feature_type transit_stair;
  bool entering_level;
#ifdef USE_TILE
  coord_def last_clicked_grid; // The map position the player last clicked on.
  int last_clicked_item; // The inventory cell the player last clicked on.
#endif

  // Warning: these two are quite different.
  //
  // The spell table is an index to a specific spell slot (you.spells).
  // The ability table lists the ability (ABIL_*) which prefers that letter.
  //
  // In other words, the spell table contains hard links and the ability
  // table contains soft links.
  FixedVector<int, 52>  spell_letter_table;   // ref to spell by slot
  FixedVector<ability_type, 52>  ability_letter_table; // ref to abil by enum

  std::set<std::string> uniq_map_tags;
  std::set<std::string> uniq_map_names;

  PlaceInfo global_info;
  player_quiver* m_quiver;

  int         escaped_death_cause;
  std::string escaped_death_aux;

  CrawlHashTable props;

  // When other levels are loaded (e.g. viewing), is the player on this level?
  bool on_current_level;

  // monsters mesmerising player; should be proteced, but needs to be saved
  // and restored.
  std::vector<int> beholders;

#if defined(WIZARD) || defined(DEBUG)
  // If set to true, then any call to ouch() which would cuase the player
  // to die automatically returns without ending the game.
  bool never_die;
#endif

protected:
    FixedVector<PlaceInfo, NUM_BRANCHES>             branch_info;
    FixedVector<PlaceInfo, NUM_LEVEL_AREA_TYPES - 1> non_branch_info;

public:
    player();
    player(const player &other);
    ~player();

    void copy_from(const player &other);

    void init();

    // Set player position without updating view geometry.
    void set_position(const coord_def &c);
    // Low-level move the player. Use this instead of changing pos directly.
    void moveto(const coord_def &c);
    bool move_to_pos(const coord_def &c);
    // Move the player during an abyss shift.
    void shiftto(const coord_def &c);
    bool blink_to(const coord_def& c, bool quiet = false);

    void reset_prev_move();

    char stat(stat_type stat, bool nonneg=true) const;
    char strength() const;
    char intel() const;
    char dex() const;
    char max_stat(stat_type stat) const;
    char max_strength() const;
    char max_intel() const;
    char max_dex() const;

    bool in_water() const;
    bool can_swim(bool permanently = false) const;
    int visible_igrd(const coord_def&) const;
    bool is_levitating() const;
    bool cannot_speak() const;
    bool invisible() const;
    bool misled() const;
    bool can_see_invisible() const;
    bool can_see_invisible(bool unid) const;
    bool visible_to(const actor *looker) const;
    bool can_see(const actor* a) const;

    // Is c in view but behind a transparent wall?
    bool trans_wall_blocking(const coord_def &c) const;
    // Override LOS for arena.
    void set_arena_los(const coord_def &c);

    bool is_icy() const;
    bool is_fiery() const;
    bool is_skeletal() const;

    bool light_flight() const;
    bool travelling_light() const;

    // Dealing with beholders. Implemented in behold.cc.
    void add_beholder(const monsters *mon);
    bool beheld() const;
    bool beheld_by(const monsters *mon) const;
    monsters* get_beholder(const coord_def &pos) const;
    monsters* get_any_beholder() const;
    void remove_beholder(const monsters *mon);
    void clear_beholders();
    void beholders_check_noise(int loudness);
    void update_beholders();
    void update_beholder(const monsters *mon);

    kill_category kill_alignment() const;

    bool has_spell(spell_type spell) const;

    size_type transform_size(int psize = PSIZE_TORSO) const;
    std::string shout_verb() const;

    item_def *slot_item(equipment_type eq,
                        bool include_melded=false);
    const item_def *slot_item(equipment_type eq,
                              bool include_melded=false) const;

    // actor
    monster_type id() const;
    int mindex() const;
    int       get_experience_level() const;
    actor_type atype() const { return ACT_PLAYER; }
    monsters* as_monster() { return NULL; }
    player* as_player() { return this; }
    const monsters* as_monster() const { return NULL; }
    const player* as_player() const { return this; }

    god_type  deity() const;
    bool      alive() const;
    bool      is_summoned(int* duration = NULL, int* summon_type = NULL) const;

    bool      swimming() const;
    bool      submerged() const;
    bool      floundering() const;
    bool      extra_balanced() const;
    bool      can_pass_through_feat(dungeon_feature_type grid) const;
    bool      is_habitable_feat(dungeon_feature_type actual_grid) const;
    size_type body_size(size_part_type psize = PSIZE_TORSO, bool base = false) const;
    int       body_weight(bool base = false) const;
    int       total_weight() const;
    int       damage_brand(int which_attack = -1);
    int       damage_type(int which_attack = -1);

    int       has_claws(bool allow_tran = true) const;
    bool      has_usable_claws(bool allow_tran = true) const;
    int       has_talons(bool allow_tran = true) const;
    bool      has_usable_talons(bool allow_tran = true) const;
    int       has_fangs(bool allow_tran = true) const;
    int       has_usable_fangs(bool allow_tran = true) const;
    int       has_tail(bool allow_tran = true) const;
    int       has_usable_tail(bool allow_tran = true) const;
    bool      has_usable_offhand() const;

    item_def *weapon(int which_attack = -1);
    item_def *shield();

    bool      can_wield(const item_def &item,
                        bool ignore_curse = false,
                        bool ignore_brand = false,
                        bool ignore_shield = false,
                        bool ignore_transform = false) const;
    bool      could_wield(const item_def &item,
                          bool ignore_brand = false,
                          bool ignore_transform = false) const;

    std::string name(description_level_type type,
                     bool force_visible = false) const;
    std::string pronoun(pronoun_type pro, bool force_visible = false) const;
    std::string conj_verb(const std::string &verb) const;
    std::string hand_name(bool plural, bool *can_plural = NULL) const;
    std::string foot_name(bool plural, bool *can_plural = NULL) const;
    std::string arm_name(bool plural, bool *can_plural = NULL) const;

    bool fumbles_attack(bool verbose = true);
    bool cannot_fight() const;

    void attacking(actor *other);
    bool can_go_berserk() const;
    bool can_go_berserk(bool intentional, bool potion = false) const;
    void go_berserk(bool intentional, bool potion = false);
    bool berserk() const;
    bool can_mutate() const;
    bool can_safely_mutate() const;
    bool can_bleed() const;
    bool mutate();
    void backlight();
    void banish(const std::string &who = "");
    void blink(bool allow_partial_control = true);
    void teleport(bool right_now = false,
                  bool abyss_shift = false,
                  bool wizard_tele = false);
    void drain_stat(stat_type stat, int amount, actor* attacker);

    void expose_to_element(beam_type element, int strength = 0);
    void god_conduct(conduct_type thing_done, int level);

    int hunger_level() const { return hunger_state; }
    void make_hungry(int nutrition, bool silent = true);
    void poison(actor *agent, int amount = 1);
    bool sicken(int amount);
    void paralyse(actor *, int str);
    void petrify(actor *, int str);
    void slow_down(actor *, int str);
    void confuse(actor *, int strength);
    bool heal(int amount, bool max_too = false);
    bool drain_exp(actor *, bool quiet = false, int pow = 3);
    bool rot(actor *, int amount, int immediate = 0, bool quiet = false);
    int hurt(const actor *attacker, int amount,
             beam_type flavour = BEAM_MISSILE,
             bool cleanup_dead = true);

    int warding() const;

    int mons_species() const;

    mon_holy_type holiness() const;
    bool undead_or_demonic() const;
    bool is_holy() const;
    bool is_unholy() const;
    bool is_evil() const;
    bool is_chaotic() const;
    bool is_insubstantial() const;
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
    int res_magic() const;
    bool confusable() const;
    bool slowable() const;

    flight_type flight_mode() const;
    bool permanent_levitation() const;
    bool permanent_flight() const;

    bool paralysed() const;
    bool cannot_move() const;
    bool cannot_act() const;
    bool confused() const;
    bool caught() const;
    bool backlit(bool check_haloed = true, bool self_halo = true) const;
    int halo_radius2() const;
    int silence_radius2() const;
    bool petrified() const;
    bool incapacitated() const
    {
        return actor::incapacitated() || stat_zero[STAT_DEX];
    };

    bool asleep() const;
    void hibernate(int power = 0);
    void put_to_sleep(actor *, int power = 0);
    void awake();
    void check_awaken(int disturbance);

    bool can_throw_large_rocks() const;
    bool can_smell() const;

    int armour_class() const;
    int gdr_perc() const;
    int melee_evasion(const actor *attacker,
                      ev_ignore_type evit = EV_IGNORE_NONE) const;

    int stat_hp() const     { return hp; }
    int stat_maxhp() const  { return hp_max; }

    int shield_bonus() const;
    int shield_block_penalty() const;
    int shield_bypass_ability(int tohit) const;

    void shield_block_succeeded(actor *foe);

    bool wearing_light_armour(bool with_skill = false) const;
    void exercise(skill_type skill, int qty);
    int  skill(skill_type skill, bool skill_bump = false) const;

    bool do_shaft();

    void apply_location_effects(const coord_def &oldpos,
                                killer_type killer = KILL_NONE,
                                int killernum = -1);

    ////////////////////////////////////////////////////////////////

    PlaceInfo& get_place_info() const ; // Current place info
    PlaceInfo& get_place_info(branch_type branch,
                              level_area_type level_type2) const;
    PlaceInfo& get_place_info(branch_type branch) const;
    PlaceInfo& get_place_info(level_area_type level_type2) const;

    void goto_place(const level_id &level);

    void set_place_info(PlaceInfo info);
    // Returns copies of the PlaceInfo; modifying the vector won't
    // modify the player object.
    std::vector<PlaceInfo> get_all_place_info(bool visited_only = false,
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



protected:
    void _removed_beholder();
    bool _possible_beholder(const monsters *mon) const;
};

extern player you;

struct player_save_info
{
    std::string name;
    unsigned long experience;
    int experience_level;
    bool wizard;
    species_type species;
    std::string class_name;
    god_type religion;
    std::string second_god_name;
#ifdef USE_TILE
    dolls_data doll;
    bool held_in_net;
#endif

    player_save_info operator=(const player& rhs);
    bool operator<(const player_save_info& rhs) const;
    std::string short_desc() const;
};

class monsters;
struct item_def;

// Helper. Use move_player_to_grid or player::apply_location_effects instead.
void moveto_location_effects(dungeon_feature_type old_feat,
                             bool stepped=false, bool allow_shift=true,
                             const coord_def& old_pos=coord_def());

bool move_player_to_grid( const coord_def& p, bool stepped, bool allow_shift,
                          bool force, bool swapping = false );

bool player_in_mappable_area(void);
bool player_in_branch(int branch);
bool player_in_hell(void);

bool berserk_check_wielded_weapon(void);
int player_equip( equipment_type slot, int sub_type, bool calc_unid = true );
int player_equip_ego_type( int slot, int sub_type );
bool player_equip_unrand( int unrand_index );
bool player_can_hit_monster(const monsters *mon);

bool player_is_shapechanged(void);

bool is_effectively_light_armour(const item_def *item);
bool player_effectively_in_light_armour();

bool player_under_penance(void);

bool extrinsic_amulet_effect(jewellery_type amulet);
bool wearing_amulet(jewellery_type which_am, bool calc_unid = true);

int burden_change(void);

int carrying_capacity(burden_state_type bs = BS_OVERLOADED);

int check_stealth(void);

int player_energy(void);

int player_adjusted_shield_evasion_penalty(int scale);
int player_adjusted_body_armour_evasion_penalty(int scale);
int player_armour_shield_spell_penalty();
int player_evasion(ev_ignore_type evit = EV_IGNORE_NONE);

int player_movement_speed(bool ignore_burden = false);

int player_hunger_rate(void);

int calc_hunger(int food_cost);

int player_icemail_armour_class();

int player_mag_abil(bool is_weighted);
int player_magical_power(void);

int player_prot_life(bool calc_unid = true, bool temp = true,
                     bool items = true);

int player_regen(void);

int player_res_cold(bool calc_unid = true, bool temp = true,
                    bool items = true);
int player_res_acid(bool calc_unid = true, bool items = true);
int player_acid_resist_factor();

int player_res_torment(bool calc_unid = true, bool temp = true);

bool player_item_conserve(bool calc_unid = true);
int player_mental_clarity(bool calc_unid = true, bool items = true);
int player_spirit_shield(bool calc_unid = true);

bool player_likes_chunks(bool permanently = false);
bool player_likes_water(bool permanently = false);

int player_mutation_level(mutation_type mut);

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

bool player_control_teleport(bool calc_unid = true, bool temp = true,
                             bool items = true);

int player_shield_class(void);


int player_spec_air(void);
int player_spec_cold(void);
int player_spec_conj(void);
int player_spec_death(void);
int player_spec_earth(void);
int player_spec_ench(void);
int player_spec_fire(void);
int player_spec_holy(void);
int player_spec_poison(void);
int player_spec_summ(void);

int player_speed(void);
int player_ponderousness();

int player_spell_levels(void);

int player_sust_abil(bool calc_unid = true);

int player_teleport(bool calc_unid = true);

bool items_give_ability(const int slot, artefact_prop_type abil);
int scan_artefacts(artefact_prop_type which_property, bool calc_unid = true);

int slaying_bonus(char which_affected, bool ranged = false);

unsigned long exp_needed(int lev);

int get_expiration_threshold(duration_type dur);
bool dur_expiring(duration_type dur);
void display_char_status(void);

void forget_map(unsigned char chance_forgotten = 100, bool force = false);

void gain_exp(unsigned int exp_gained, unsigned int* actual_gain = NULL,
              unsigned int* actual_avail_gain = NULL);

bool player_in_bat_form();
bool player_can_open_doors();

inline bool player_can_handle_equipment()
{
    return player_can_open_doors();
}

void level_change(bool skip_attribute_increase = false);

bool player_genus( genus_type which_genus,
                   species_type species = SP_UNKNOWN );
bool is_player_same_species( const int mon, bool = false );

bool you_can_wear( int eq, bool special_armour = false );
bool player_has_feet(void);
bool player_wearing_slot( int eq );
bool you_tran_can_wear(const item_def &item);
bool you_tran_can_wear( int eq, bool check_mutation = false );

bool enough_hp(int minimum, bool suppress_msg);
bool enough_mp(int minimum, bool suppress_msg, bool include_items = true);

void dec_hp(int hp_loss, bool fatal, const char *aux = NULL);
void dec_mp(int mp_loss);

void inc_mp(int mp_gain, bool max_too);
void inc_hp(int hp_gain, bool max_too);

void rot_hp(int hp_loss);
void unrot_hp(int hp_recovered);
int player_rotted();
void rot_mp(int mp_loss);

void inc_max_hp( int hp_gain );
void dec_max_hp( int hp_loss );

void inc_max_mp( int mp_gain );
void dec_max_mp( int mp_loss );

void deflate_hp(int new_level, bool floor);
void set_hp(int new_amount, bool max_too);

int get_real_hp(bool trans, bool rotted = false);
int get_real_mp(bool include_items);

int get_contamination_level();

void set_mp(int new_amount, bool max_too);

void contaminate_player(int change, bool controlled = false,
                        bool status_only = false);

bool confuse_player(int amount, bool resistable = true);

bool curare_hits_player(int death_source, int amount, const bolt &beam);
bool poison_player(int amount, std::string source,
                   std::string source_aux = "", bool force = false);
void dec_poison_player();
void reduce_poison_player(int amount);
bool miasma_player(std::string source, std::string source_aux = "");

bool napalm_player(int amount);
void dec_napalm_player(int delay);

bool slow_player(int turns);
void dec_slow_player(int delay);
void dec_exhaust_player(int delay);

bool haste_player(int turns);
void dec_haste_player(int delay);

void dec_disease_player(int delay);

bool player_weapon_wielded();

// Determines if the given grid is dangerous for the player to enter.
bool is_feat_dangerous(dungeon_feature_type feat);

void run_macro(const char *macroname = NULL);

int count_worn_ego(int which_ego);

#endif
