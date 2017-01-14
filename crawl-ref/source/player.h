/**
 * @file
 * @brief Player related functions.
**/


#ifndef PLAYER_H
#define PLAYER_H

#include <chrono>
#include <list>
#include <memory>
#include <vector>

#include "actor.h"
#include "beam.h"
#include "bitary.h"
#include "kills.h"
#include "place-info.h"
#include "quiver.h"
#include "religion-enum.h"
#include "species.h"
#ifdef USE_TILE
#include "tiledoll.h"
#endif

#define ICY_ARMOUR_KEY "ozocubu's_armour_pow"
#define TRANSFORM_POW_KEY "transform_pow"
#define BARBS_MOVE_KEY "moved_with_barbs_status"
#define HORROR_PENALTY_KEY "horror_penalty"
#define POWERED_BY_DEATH_KEY "powered_by_death_strength"
#define SONG_OF_SLAYING_KEY "song_of_slaying_bonus"
#define FORCE_MAPPABLE_KEY "force_mappable"
#define REGEN_AMULET_ACTIVE "regen_amulet_active"
#define MANA_REGEN_AMULET_ACTIVE "mana_regen_amulet_active"
#define SAP_MAGIC_KEY "sap_magic_amount"
#define TEMP_WATERWALK_KEY "temp_waterwalk"
#define EMERGENCY_FLIGHT_KEY "emergency_flight"

// display/messaging breakpoints for penalties from Ru's MUT_HORROR
#define HORROR_LVL_EXTREME  3
#define HORROR_LVL_OVERWHELMING  5

#define SEVERE_CONTAM_LEVEL 3

/// Maximum stat value
static const int MAX_STAT_VALUE = 125;
/// The standard unit of regen; one level in artifact inscriptions
static const int REGEN_PIP = 40;
/// The standard unit of MR; one level in %/@ screens
static const int MR_PIP = 40;
/// The standard unit of stealth; one level in %/@ screens
static const int STEALTH_PIP = 50;

/// The rough number of aut getting hit takes off your bone armour
static const int BONE_ARMOUR_HIT_RATIO = 50;

/// The minimum aut cost for a player move (before haste)
static const int FASTEST_PLAYER_MOVE_SPEED = 6;
// relevant for swiftness, etc

// Min delay for thrown projectiles.
static const int FASTEST_PLAYER_THROWING_SPEED = 7;

class targeter;
class Delay;

int player_stealth();

/// used for you.train[] & for rendering skill tiles (tileidx_skill)
enum training_status
{
    TRAINING_DISABLED = 0,
    TRAINING_ENABLED,
    TRAINING_FOCUSED,
    NUM_TRAINING_STATUSES,
    // the below are only used for display purposes, not training.
    TRAINING_MASTERED,
    TRAINING_INACTIVE, ///< enabled but not used (in auto mode)
};

// needed for assert in is_player()
#ifdef DEBUG_GLOBALS
#define you (*real_you)
#endif
extern player you;

typedef FixedVector<int, NUM_DURATIONS> durations_t;
class player : public actor
{
public:
    // ---------------
    // Character save chunk data:
    // None of this is really necessary, except for some complicated
    // hacks with player_save_info. Should only be used in tags.cc or
    // player_save_info::operator=(player).
    // ---------------
    string chr_species_name;
    string chr_class_name;
    string chr_god_name;

    // ---------------
    // Permanent data:
    // ---------------
    string your_name;
    species_type species;
    job_type char_class;

    // This field is here even in non-WIZARD compiles, since the
    // player might have been playing previously under wiz mode.
    bool          wizard;            // true if player has entered wiz mode.
    bool          explore;           // true if player has entered explore mode.
    time_t        birth_time;        // start time of game

    // ----------------
    // Long-term state:
    // ----------------
    int elapsed_time;        // total amount of elapsed time in the game
    int elapsed_time_at_last_input; // used for elapsed_time delta display

    int hp;
    int hp_max;
    int hp_max_adj_temp;        // temporary max HP loss (rotting)
    int hp_max_adj_perm;        // base HPs from background (and permanent loss)

    int magic_points;
    int max_magic_points;
    int mp_max_adj;             // max MP loss (ability costs, tutorial bonus)

    FixedVector<int8_t, NUM_STATS> stat_loss;
    FixedVector<int8_t, NUM_STATS> base_stats;

    int hunger;
    int disease;
    hunger_state_t hunger_state;
    uint8_t max_level;
    int hit_points_regeneration;
    int magic_points_regeneration;
    unsigned int experience;
    unsigned int total_experience; // Unaffected by draining. Used for skill cost.
    int experience_level;
    int gold;
    int zigs_completed, zig_max;

    FixedVector<int8_t, NUM_EQUIP> equip;
    FixedBitVector<NUM_EQUIP> melded;
    // Whether these are unrands that we should run the _*_world_reacts func for
    FixedBitVector<NUM_EQUIP> unrand_reacts;

    FixedArray<int, NUM_OBJECT_CLASSES, MAX_SUBTYPES> force_autopickup;

    // PC's symbol (usually @) and colour.
    monster_type symbol;
    transformation form;

    FixedVector< item_def, ENDOFPACK > inv;
    FixedBitVector<NUM_RUNE_TYPES> runes;
    int obtainable_runes; // can be != 15 in Sprint

    FixedVector<spell_type, MAX_KNOWN_SPELLS> spells;
    set<spell_type> old_vehumet_gifts, vehumet_gifts;

    uint8_t spell_no;
    game_chapter chapter;
    bool royal_jelly_dead;
    bool transform_uncancellable;
    bool fishtail; // Merfolk fishtail transformation

    unsigned short pet_target;

    durations_t duration;
    int rotting;
    bool apply_berserk_penalty;         // Whether to apply the berserk penalty at
    // end of the turn.
    int berserk_penalty;                // The penalty for moving while berserk

    FixedVector<int, NUM_ATTRIBUTES> attribute;
    FixedVector<uint8_t, NUM_AMMO> quiver; // default items for quiver
    FixedVector<int, NUM_TIMERS> last_timer_effect;
    FixedVector<int, NUM_TIMERS> next_timer_effect;

    bool pending_revival;
    int lives;
    int deaths;
#if TAG_MAJOR_VERSION == 34
    float temperature; // For lava orcs.
    float temperature_last;
#endif

    FixedVector<uint8_t, NUM_SKILLS> skills; ///< skill level
    FixedVector<training_status, NUM_SKILLS> train; ///< see enum def
    FixedVector<training_status, NUM_SKILLS> train_alt; ///< config of other mode
    FixedVector<unsigned int, NUM_SKILLS>  training; ///< percentage of XP used
    FixedBitVector<NUM_SKILLS> can_train; ///< Is training this skill allowed?
    FixedVector<unsigned int, NUM_SKILLS> skill_points;

    /// track skill points gained by crosstraining
    FixedVector<unsigned int, NUM_SKILLS> ct_skill_points;
    FixedVector<uint8_t, NUM_SKILLS>  skill_order;

    bool auto_training;
    list<skill_type> exercises;     ///< recent practise events
    list<skill_type> exercises_all; ///< also include events for disabled skills
    set<skill_type> stop_train;     ///< need to check if we can still train
    set<skill_type> start_train;    ///< we can resume training

    // Skill menu states
    skill_menu_state skill_menu_do;
    skill_menu_state skill_menu_view;

    //Ashenzari transfer knowledge
    skill_type    transfer_from_skill;
    skill_type    transfer_to_skill;
    unsigned int  transfer_skill_points;
    unsigned int  transfer_total_skill_points;

    int  skill_cost_level;
    int  exp_available;

    FixedVector<int, NUM_GODS> exp_docked;
    FixedVector<int, NUM_GODS> exp_docked_total; // XP-based wrath

    FixedArray<uint32_t, 6, MAX_SUBTYPES> item_description;
    FixedVector<unique_item_status_type, MAX_UNRANDARTS> unique_items;
    FixedBitVector<NUM_MONSTERS> unique_creatures;

    KillMaster kills;

    branch_type where_are_you;
    int depth;

    FixedVector<uint8_t, 30> branch_stairs;

    god_type religion;
    string jiyva_second_name;       // Random second name of Jiyva
    uint8_t piety;
    uint8_t piety_hysteresis;       // amount of stored-up docking
    uint8_t gift_timeout;
    uint8_t saved_good_god_piety;   // for if you "switch" between E/Z/1 by abandoning one first
    god_type previous_good_god;
    FixedVector<uint8_t, NUM_GODS>  penance;
    FixedVector<uint8_t, NUM_GODS>  worshipped;
    FixedVector<short,   NUM_GODS>  num_current_gifts;
    FixedVector<short,   NUM_GODS>  num_total_gifts;
    FixedBitVector<      NUM_GODS>  one_time_ability_used;
    FixedVector<uint8_t, NUM_GODS>  piety_max;

    FixedVector<uint8_t, NUM_MUTATIONS> mutation;
    FixedVector<uint8_t, NUM_MUTATIONS> innate_mutation;
    FixedVector<uint8_t, NUM_MUTATIONS> temp_mutation;
    FixedVector<uint8_t, NUM_MUTATIONS> sacrifices;

    FixedVector<uint8_t, NUM_ABILITIES> sacrifice_piety;

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

    int real_time() { return real_time_ms.count() / 1000; }
    chrono::milliseconds real_time_ms;       // real time played
    chrono::milliseconds real_time_delta;    // real time since last command

    int num_turns;            // number of turns taken
    int exploration;          // levels explored (16.16 bit real number)

    int                       last_view_update;     // what turn was the view last updated?

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
    player_quiver m_quiver;

    // monsters mesmerising player; should be protected, but needs to be saved
    // and restored.
    vector<mid_t> beholders;

    // monsters causing fear to the player; see above
    vector<mid_t> fearmongers;

    // Delayed level actions. This array is never trimmed, as usually D:1 won't
    // be loaded again until the very end.
    vector<daction_type> dactions;

    // Path back from portal levels.
    vector<level_pos> level_stack;

    // The player's knowledge about item types.
    id_arr type_ids;

    // The version the save was last played with.
    string prev_save_version;

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

    // Coordinates of last travel target; note that this is never used by
    // travel itself, only by the level-map to remember the last travel target.
    short travel_x, travel_y;
    level_id travel_z;

    runrest running;                    // Nonzero if running/traveling.
    bool travel_ally_pace;

    bool received_weapon_warning;
    bool received_noskill_warning;
    bool wizmode_teleported_into_rock;

    // This should really be unique_ptr, but that causes issues since files.cc
    // uses the default constructor of `player`.
    vector<shared_ptr<Delay>> delay_queue; // pending actions

    chrono::time_point<chrono::system_clock> last_keypress_time;

    bool xray_vision;
    int8_t bondage_level;  // how much an Ash worshipper is into bondage
    int8_t bondage[NUM_ET];
    map<skill_type, int8_t> skill_boost; // Skill bonuses.
    bool digging;

    // The last spell cast by the player.
    spell_type last_cast_spell;
    map<int,int> last_pickup;


    // ---------------------------
    // Volatile (same-turn) state:
    // ---------------------------
    bool turn_is_over; // player has performed a timed action

    // If true, player is headed to the Abyss.
    bool banished;
    string banished_by;
    int banished_power;

    bool wield_change;          // redraw weapon
    bool redraw_quiver;         // redraw quiver

    bool redraw_title;
    bool redraw_hit_points;
    bool redraw_magic_points;
#if TAG_MAJOR_VERSION == 34
    bool redraw_temperature;
#endif
    FixedVector<bool, NUM_STATS> redraw_stats;
    bool redraw_experience;
    bool redraw_armour_class;
    bool redraw_evasion;
    bool redraw_status_lights;

    colour_t flash_colour;
    targeter *flash_where;

    int time_taken;

    int old_hunger;            // used for hunger delta-meter (see output.cc)

    // Set when the character is going to a new level, to guard against levgen
    // failures
    dungeon_feature_type transit_stair;
    bool entering_level;

    int    escaped_death_cause;
    string escaped_death_aux;

    int turn_damage;   // cumulative damage per turn
    mid_t damage_source; // death source of last damage done to player
    int source_damage; // cumulative damage for you.damage_source

    // When other levels are loaded (e.g. viewing), is the player on this level?
    bool on_current_level;

    // View code clears and needs new data in places where we can't announce
    // the portal right away; delay the announcements then.
    int seen_portals;

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
    virtual ~player();

    void init_skills();

    // Set player position without updating view geometry.
    void set_position(const coord_def &c) override;
    // Low-level move the player. Use this instead of changing pos directly.
    void moveto(const coord_def &c, bool clear_net = true) override;
    bool move_to_pos(const coord_def &c, bool clear_net = true,
                     bool /*force*/ = false) override;
    // Move the player during an abyss shift.
    void shiftto(const coord_def &c);
    bool blink_to(const coord_def& c, bool quiet = false) override;

    int stat(stat_type stat, bool nonneg = true) const;
    int strength(bool nonneg = true) const;
    int intel(bool nonneg = true) const;
    int dex(bool nonneg = true) const;
    int max_stat(stat_type stat, bool base = false) const;
    int max_strength() const;
    int max_intel() const;
    int max_dex() const;

    bool in_water() const;
    bool in_lava() const;
    bool in_liquid() const;
    bool can_swim(bool permanently = false) const;
    bool can_water_walk() const;
    int visible_igrd(const coord_def&) const;
    bool can_cling_to_walls() const override;
    bool is_banished() const override;
    bool is_sufficiently_rested() const; // Up to rest_wait_percent HP and MP.
    bool is_web_immune() const override;
    bool cannot_speak() const;
    bool invisible() const override;
    bool can_see_invisible(bool calc_unid = true) const override;
    bool innate_sinv() const;
    bool visible_to(const actor *looker) const override;
    bool can_see(const actor& a) const override;
    undead_state_type undead_state(bool temp = true) const;
    bool nightvision() const override;
    reach_type reach_range() const override;
    bool see_cell(const coord_def& p) const override;

    // Is c in view but behind a transparent wall?
    bool trans_wall_blocking(const coord_def &c) const;

    bool is_icy() const override;
    bool is_fiery() const override;
    bool is_skeletal() const override;

    bool tengu_flight() const;
    int heads() const override;

    bool spellcasting_unholy() const;

    // Dealing with beholders. Implemented in behold.cc.
    void add_beholder(const monster& mon, bool axe = false);
    bool beheld() const;
    bool beheld_by(const monster& mon) const;
    monster* get_beholder(const coord_def &pos) const;
    monster* get_any_beholder() const;
    void remove_beholder(const monster& mon);
    void clear_beholders();
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
    void update_fearmongers();
    void update_fearmonger(const monster* mon);

    bool made_nervous_by(const coord_def &pos);

    kill_category kill_alignment() const override;

    bool has_spell(spell_type spell) const override;

    string shout_verb(bool directed = false) const;
    int shout_volume() const;

    item_def *slot_item(equipment_type eq, bool include_melded=false) const
        override;

    int base_ac_from(const item_def &armour, int scale = 1) const;
    void maybe_degrade_bone_armour(int trials);

    int inaccuracy() const override;

    // actor
    int mindex() const override;
    int get_hit_dice() const override;
    int get_experience_level() const override;
    int get_max_xl() const;
    bool is_player() const override
    {
#ifndef DEBUG_GLOBALS
        ASSERT(this == (actor*)&you); // there can be only one
#endif
        return true;
    }
    monster* as_monster() override { return nullptr; }
    player* as_player() override { return this; }
    const monster* as_monster() const override { return nullptr; }
    const player* as_player() const override { return this; }

    god_type  deity() const override;
    bool      alive() const override;
    bool      is_summoned(int* duration = nullptr,
                          int* summon_type = nullptr) const override;
    bool      is_perm_summoned() const override { return false; };

    bool        swimming() const override;
    bool        submerged() const override;
    bool        floundering() const override;
    bool        extra_balanced() const override;
    bool        shove(const char* feat_name = "") override;
    bool        can_pass_through_feat(dungeon_feature_type grid) const override;
    bool        is_habitable_feat(dungeon_feature_type actual_grid) const
        override;
    size_type   body_size(size_part_type psize = PSIZE_TORSO,
                          bool base = false) const override;
    brand_type  damage_brand(int which_attack = -1) override;
    int         damage_type(int which_attack = -1) override;
    random_var  attack_delay(const item_def *projectile = nullptr,
                             bool rescale = true) const override;
    int         constriction_damage() const override;

    int       has_claws(bool allow_tran = true) const override;
    bool      has_usable_claws(bool allow_tran = true) const;
    int       has_talons(bool allow_tran = true) const;
    bool      has_usable_talons(bool allow_tran = true) const;
    int       has_hooves(bool allow_tran = true) const;
    bool      has_usable_hooves(bool allow_tran = true) const;
    int       has_fangs(bool allow_tran = true) const;
    int       has_usable_fangs(bool allow_tran = true) const;
    int       has_tail(bool allow_tran = true) const;
    int       has_usable_tail(bool allow_tran = true) const;
    bool      has_usable_offhand() const;
    int       has_pseudopods(bool allow_tran = true) const;
    int       has_usable_pseudopods(bool allow_tran = true) const;
    int       has_tentacles(bool allow_tran = true) const;
    int       has_usable_tentacles(bool allow_tran = true) const;

    int wearing(equipment_type slot, int sub_type, bool calc_unid = true) const
        override;
    int wearing_ego(equipment_type slot, int type, bool calc_unid = true) const
        override;
    int scan_artefacts(artefact_prop_type which_property,
                       bool calc_unid = true,
                       vector<item_def> *matches = nullptr) const override;

    item_def *weapon(int which_attack = -1) const override;
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

    string name(description_level_type type, bool force_visible = false,
                bool force_article = false) const override;
    string pronoun(pronoun_type pro, bool force_visible = false) const override;
    string conj_verb(const string &verb) const override;
    string hand_name(bool plural, bool *can_plural = nullptr) const override;
    string hands_verb(const string &plural_verb) const;
    string hands_act(const string &plural_verb, const string &object) const;
    string foot_name(bool plural, bool *can_plural = nullptr) const override;
    string arm_name(bool plural, bool *can_plural = nullptr) const override;
    string unarmed_attack_name() const;

    bool fumbles_attack() override;
    bool fights_well_unarmed(int heavy_armour_penalty) override;

    void attacking(actor *other, bool ranged = false) override;
    bool can_go_berserk() const override;
    bool can_go_berserk(bool intentional, bool potion = false,
                        bool quiet = false, string *reason = nullptr) const;
    bool go_berserk(bool intentional, bool potion = false) override;
    bool berserk() const override;
    bool can_mutate() const override;
    bool can_safely_mutate(bool temp = true) const override;
    bool is_lifeless_undead(bool temp = true) const;
    bool can_polymorph() const override;
    bool can_bleed(bool allow_tran = true) const override;
    bool is_stationary() const override;
    bool malmutate(const string &reason) override;
    bool polymorph(int pow) override;
    void backlight();
    void banish(actor* /*agent*/, const string &who = "", const int power = 0,
                bool force = false) override;
    void blink() override;
    void teleport(bool right_now = false,
                  bool wizard_tele = false) override;
    void drain_stat(stat_type stat, int amount) override;

    void expose_to_element(beam_type element, int strength = 0,
                           bool slow_cold_blood = true) override;
    void god_conduct(conduct_type thing_done, int level) override;

    void make_hungry(int nutrition, bool silent = true) override;
    bool poison(actor *agent, int amount = 1, bool force = false) override;
    bool sicken(int amount) override;
    void paralyse(actor *, int str, string source = "") override;
    void petrify(actor *, bool force = false) override;
    bool fully_petrify(actor *foe, bool quiet = false) override;
    void slow_down(actor *, int str) override;
    void confuse(actor *, int strength) override;
    void weaken(actor *attacker, int pow) override;
    bool heal(int amount) override;
    bool drain_exp(actor *, bool quiet = false, int pow = 3) override;
    bool rot(actor *, int amount, bool quiet = false, bool no_cleanup = false)
        override;
    void splash_with_acid(const actor* evildoer, int acid_strength,
                          bool allow_corrosion = true,
                          const char* hurt_msg = nullptr) override;
    bool corrode_equipment(const char* corrosion_source = "the acid",
                           int degree = 1) override;
    void sentinel_mark(bool trap = false);
    int hurt(const actor *attacker, int amount,
             beam_type flavour = BEAM_MISSILE,
             kill_method_type kill_type = KILLED_BY_MONSTER,
             string source = "",
             string aux = "",
             bool cleanup_dead = true,
             bool attacker_effects = true) override;

    bool wont_attack() const override { return true; };
    mon_attitude_type temp_attitude() const override { return ATT_FRIENDLY; };
    mon_attitude_type real_attitude() const override { return ATT_FRIENDLY; };

    monster_type mons_species(bool zombie_base = false) const override;

    mon_holy_type holiness(bool temp = true) const override;
    bool undead_or_demonic() const override;
    bool is_holy(bool spells = true) const override;
    bool is_nonliving(bool temp = true) const override;
    int how_chaotic(bool check_spells_god) const override;
    bool is_unbreathing() const override;
    bool is_insubstantial() const override;
    int res_acid(bool calc_unid = true) const override;
    bool res_damnation() const override { return false; };
    int res_fire() const override;
    int res_steam() const override;
    int res_cold() const override;
    int res_elec() const override;
    int res_poison(bool temp = true) const override;
    int res_rotting(bool temp = true) const override;
    int res_water_drowning() const override;
    bool res_sticky_flame() const override;
    int res_holy_energy() const override;
    int res_negative_energy(bool intrinsic_only = false) const override;
    bool res_torment() const override;
    bool res_wind() const override;
    bool res_petrify(bool temp = true) const override;
    int res_constrict() const override;
    int res_magic(bool /*calc_unid*/ = true) const override;
    bool no_tele(bool calc_unid = true, bool /*permit_id*/ = true,
                 bool blink = false) const override;
    string no_tele_reason(bool calc_unid = true, bool blink = false) const;
    bool no_tele_print_reason(bool calc_unid = true, bool blink = false) const;
    bool antimagic_susceptible() const override;

    bool gourmand(bool calc_unid = true, bool items = true) const override;
    bool res_corr(bool calc_unid = true, bool items = true) const override;
    bool clarity(bool calc_unid = true, bool items = true) const override;
    bool stasis() const override;

    bool airborne() const override;
    bool cancellable_flight() const;
    bool permanent_flight() const;
    bool racial_permanent_flight() const;

    bool paralysed() const override;
    bool cannot_move() const override;
    bool cannot_act() const override;
    bool confused() const override;
    bool caught() const override;
    bool backlit(bool self_halo = true) const override;
    bool umbra() const override;
    int halo_radius() const override;
    int silence_radius() const override;
    int liquefying_radius() const override;
    int umbra_radius() const override;
#if TAG_MAJOR_VERSION == 34
    int heat_radius() const override;
#endif
    bool petrifying() const override;
    bool petrified() const override;
    bool liquefied_ground() const override;
    bool incapacitated() const override
    {
        return actor::incapacitated() || duration[DUR_CLUMSY];
    }

    bool asleep() const override;
    void put_to_sleep(actor *, int power = 0, bool hibernate = false) override;
    void awaken();
    void check_awaken(int disturbance) override;
    int beam_resists(bolt &beam, int hurted, bool doEffects, string source)
        override;

    bool can_throw_large_rocks() const override;
    bool can_smell() const;
    bool can_sleep(bool holi_only = false) const override;

    int racial_ac(bool temp) const;
    int base_ac(int scale) const;
    int armour_class(bool /*calc_unid*/ = true) const override;
    int gdr_perc() const override;
    int evasion(ev_ignore_type evit = EV_IGNORE_NONE,
                const actor *attacker = nullptr) const override;

    int stat_hp() const override     { return hp; }
    int stat_maxhp() const override  { return hp_max; }
    int stealth() const override     { return player_stealth(); }

    bool shielded() const override;
    int shield_bonus() const override;
    int shield_block_penalty() const override;
    int shield_bypass_ability(int tohit) const override;
    void shield_block_succeeded(actor *foe) override;
    int missile_deflection() const override;
    void ablate_deflection() override;

    // Combat-related adjusted penalty calculation methods
    int unadjusted_body_armour_penalty() const override;
    int adjusted_body_armour_penalty(int scale = 1) const override;
    int adjusted_shield_penalty(int scale = 1) const override;
    float get_shield_skill_to_offset_penalty(const item_def &item);
    int armour_tohit_penalty(bool random_factor, int scale = 1) const override;
    int shield_tohit_penalty(bool random_factor, int scale = 1) const override;

    bool wearing_light_armour(bool with_skill = false) const;
    int  skill(skill_type skill, int scale =1,
               bool real = false, bool drained = true) const override;

    bool do_shaft() override;

    bool can_do_shaft_ability(bool quiet = false) const;
    bool do_shaft_ability();

    bool can_potion_heal();
    int scale_potion_healing(int healing_amount);

    void apply_location_effects(const coord_def &oldpos,
                                killer_type killer = KILL_NONE,
                                int killernum = -1) override;

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
                           const char* msg = nullptr);
    void set_duration(duration_type dur, int turns, int cap = 0,
                      const char *msg = nullptr);

    bool attempt_escape(int attempts = 1);
    int usable_tentacles() const;
    bool has_usable_tentacle() const override;

    bool form_uses_xl() const;

    bool clear_far_engulf() override;

protected:
    void _removed_beholder(bool quiet = false);
    bool _possible_beholder(const monster* mon) const;

    void _removed_fearmonger(bool quiet = false);
    bool _possible_fearmonger(const monster* mon) const;
};

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
                             bool stepped=false, const coord_def& old_pos=coord_def());

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

bool swap_check(monster* mons, coord_def &loc, bool quiet = false);

void move_player_to_grid(const coord_def& p, bool stepped);

bool is_map_persistent();
bool player_in_connected_branch();
bool player_in_hell();
bool player_in_starting_abyss();

static inline bool player_in_branch(int branch)
{
    return you.where_are_you == branch;
}

bool berserk_check_wielded_weapon();
bool player_equip_unrand(int unrand_index);
bool player_can_hear(const coord_def& p, int hear_distance = 999);

bool player_is_shapechanged();

bool is_effectively_light_armour(const item_def *item);
bool player_effectively_in_light_armour();

int player_energy();

int player_shield_racial_factor();
int player_armour_shield_spell_penalty();

int player_movement_speed();

int player_hunger_rate(bool temp = true);

int calc_hunger(int food_cost);

int player_icemail_armour_class();
int sanguine_armour_bonus();

int player_wizardry(spell_type spell);

int player_prot_life(bool calc_unid = true, bool temp = true,
                     bool items = true);

int player_regen();
void update_regen_amulet_attunement();
void update_mana_regen_amulet_attunement();

int player_res_cold(bool calc_unid = true, bool temp = true,
                    bool items = true);
int player_res_acid(bool calc_unid = true, bool items = true);

bool player_res_torment(bool random = true);
bool player_kiku_res_torment();

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

int player_shield_class();
int player_displayed_shield_class();
bool player_omnireflects();

int player_spec_air();
int player_spec_cold();
int player_spec_conj();
int player_spec_death();
int player_spec_earth();
int player_spec_fire();
int player_spec_hex();
int player_spec_charm();
int player_spec_poison();
int player_spec_summ();

const int player_adjust_evoc_power(const int power, int enhancers = 0);

int player_speed();

int player_spell_levels();
int player_total_spell_levels();

int player_teleport(bool calc_unid = true);

int player_monster_detect_radius();

int slaying_bonus(bool ranged = false);

unsigned int exp_needed(int lev, int exp_apt = -99);
bool will_gain_life(int lev);

bool dur_expiring(duration_type dur);
void display_char_status();

void forget_map(bool rot = false);

int get_exp_progress();
void gain_exp(unsigned int exp_gained, unsigned int* actual_gain = nullptr);

void level_change(bool skip_attribute_increase = false);
void adjust_level(int diff, bool just_xp = false);

bool is_player_same_genus(const monster_type mon);
monster_type player_mons(bool transform = true);
void update_player_symbol();
void update_vision_range();

maybe_bool you_can_wear(equipment_type eq, bool temp = false);
bool player_has_feet(bool temp = true);

bool enough_hp(int minimum, bool suppress_msg, bool abort_macros = true);
bool enough_mp(int minimum, bool suppress_msg, bool abort_macros = true);

void calc_hp();
void calc_mp();
void recalc_and_scale_hp();

void dec_hp(int hp_loss, bool fatal, const char *aux = nullptr);
void dec_mp(int mp_loss, bool silent = false);
void drain_mp(int mp_loss);

void inc_mp(int mp_gain, bool silent = false);
void inc_hp(int hp_gain);
void flush_mp();

void rot_hp(int hp_loss);
// Unrot the player and return excess HP if any.
int unrot_hp(int hp_recovered);
int player_rotted();
void rot_mp(int mp_loss);

void inc_max_hp(int hp_gain);
void dec_max_hp(int hp_loss);

void deflate_hp(int new_level, bool floor);
void set_hp(int new_amount);

int get_real_hp(bool trans, bool rotted = false);
int get_real_mp(bool include_items);

int get_contamination_level();
bool player_severe_contamination();
string describe_contamination(int level);

bool sanguine_armour_valid();
void activate_sanguine_armour();

void refresh_weapon_protection();

void set_mp(int new_amount);

bool player_regenerates_hp();
bool player_regenerates_mp();

void print_potion_heal_message();

void contaminate_player(int change, bool controlled = false, bool msg = true);

bool confuse_player(int amount, bool quiet = false, bool force = false);

bool poison_player(int amount, string source, string source_aux = "",
                   bool force = false);
void paralyse_player(string source, int amount = 0);
void handle_player_poison(int delay);
void reduce_player_poison(int amount);
int get_player_poisoning();
bool poison_is_lethal();
int poison_survival();

bool miasma_player(actor *who, string source_aux = "");

bool napalm_player(int amount, string source, string source_aux = "");
void dec_napalm_player(int delay);

bool spell_slow_player(int pow);
bool slow_player(int turns);
void dec_slow_player(int delay);
void dec_exhaust_player(int delay);

bool haste_player(int turns, bool rageext = false);
void dec_haste_player(int delay);
void dec_elixir_player(int delay);
void dec_ambrosia_player(int delay);
void dec_channel_player(int delay);
bool invis_allowed(bool quiet = false, string *fail_reason = nullptr);
bool flight_allowed(bool quiet = false, string *fail_reason = nullptr);
void fly_player(int pow, bool already_flying = false);
void float_player();
bool land_player(bool quiet = false);
void player_open_door(coord_def doorpos);
void player_close_door(coord_def doorpos);

void dec_disease_player(int delay);
void player_end_berserk();

void handle_player_drowning(int delay);

// Determines if the given grid is dangerous for the player to enter.
bool is_feat_dangerous(dungeon_feature_type feat, bool permanently = false,
                       bool ignore_flight = false);
void enable_emergency_flight();

int count_worn_ego(int which_ego);
bool need_expiration_warning(duration_type dur, dungeon_feature_type feat);
bool need_expiration_warning(dungeon_feature_type feat);
bool need_expiration_warning(duration_type dur, coord_def p = you.pos());
bool need_expiration_warning(coord_def p = you.pos());

bool player_has_orb();
bool player_on_orb_run();

#if TAG_MAJOR_VERSION == 34
enum temperature_level
{
    TEMP_MIN = 1, // Minimum (and starting) temperature. Not any warmer than bare rock.
    TEMP_COLD = 3,
    TEMP_COOL = 5,
    TEMP_ROOM = 7,
    TEMP_WARM = 9, // Warmer than most creatures.
    TEMP_HOT = 11,
    TEMP_FIRE = 13, // Hot enough to ignite paper around you.
    TEMP_MAX = 15, // Maximum temperature. As hot as lava!
};

enum temperature_effect
{
    LORC_LAVA_BOOST,
    LORC_FIRE_BOOST,
    LORC_STONESKIN,
    LORC_COLD_VULN,
    LORC_PASSIVE_HEAT,
    LORC_HEAT_AURA,
    LORC_NO_SCROLLS,
    LORC_FIRE_RES_I,
    LORC_FIRE_RES_II,
    LORC_FIRE_RES_III,
};

int temperature();
int temperature_last();
void temperature_check();
void temperature_increment(float degree);
void temperature_decrement(float degree);
void temperature_changed(float change);
void temperature_decay();
bool temperature_tier(int which);
bool temperature_effect(int which);
int temperature_colour(int temp);
string temperature_string(int temp);
string temperature_text(int temp);
#endif

#endif
