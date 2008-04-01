/*
 *  File:       externs.h
 *  Summary:    Fixed size 2D vector class that asserts if you do something bad.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <3>     7/29/00    JDJ   Renamed sh_x, sh_y, sh_greed, sh_type, sh_level so
 *                          they start with shop.
 * <2>     7/29/00    JDJ   Switched to using bounds checked array classes.
 *                          Made a few char arrays unsigned char arrays.
 */

#ifndef EXTERNS_H
#define EXTERNS_H

#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <cstdlib>
#include <deque>

#include <time.h>

#include "defines.h"
#include "enum.h"
#include "FixAry.h"
#include "libutil.h"
#include "mpr.h"
#include "store.h"

#ifdef USE_TILE
struct TileFlavor
{
    char floor;
    char wall;
    char special;
};
#endif

#define INFO_SIZE       200          // size of message buffers
#define ITEMNAME_SIZE   200          // size of item names/shop names/etc
#define HIGHSCORE_SIZE  800          // <= 10 Lines for long format scores

#define MAX_NUM_GODS    21

#define BURDEN_TO_AUM 0.1f           // scale factor for converting burden to aum

extern char info[INFO_SIZE];         // defined in acr.cc {dlb}

extern unsigned char show_green;     // defined in view.cc {dlb}

// defined in mon-util.cc -- w/o this screen redraws *really* slow {dlb}
extern FixedVector<unsigned short, 1000> mcolour;  

const int kNameLen = 30;
#ifdef SHORT_FILE_NAMES
    const int kFileNameLen = 6;
#else
    const int kFileNameLen = 250;
#endif

// Used only to bound the init file name length
const int kPathLen = 256;

// This value is used to mark that the current berserk is free from
// penalty (Xom's granted or from a deck of cards).
#define NO_BERSERK_PENALTY    -1

struct item_def;
class melee_attack;
struct coord_def;
class level_id;

class actor
{
public:
    virtual ~actor();

    virtual int       id() const = 0;
    virtual actor_type atype() const = 0;

    virtual kill_category kill_alignment() const = 0;
    virtual god_type  deity() const = 0;
    
    virtual bool      alive() const = 0;
    
    virtual coord_def pos() const = 0;
    virtual bool      swimming() const = 0;
    virtual bool      submerged() const = 0;
    virtual bool      floundering() const = 0;

    virtual int       get_experience_level() const = 0;

    virtual bool can_pass_through_feat(dungeon_feature_type grid) const = 0;
    virtual bool can_pass_through(int x, int y) const;
    virtual bool can_pass_through(const coord_def &c) const;
    
    virtual size_type body_size(int psize = PSIZE_TORSO,
                                bool base = false) const = 0;
    virtual int       body_weight() const = 0;
    virtual int       total_weight() const = 0;
    
    virtual int       damage_type(int which_attack = -1) = 0;
    virtual int       damage_brand(int which_attack = -1) = 0;
    virtual item_def *weapon(int which_attack = -1) = 0;
    virtual item_def *shield() = 0;
    virtual item_def *slot_item(equipment_type eq) = 0;
    // Just a wrapper; not to be overridden
    const item_def *slot_item(equipment_type eq) const
    {
        return const_cast<actor*>(this)->slot_item(eq);
    }
    virtual bool has_equipped(equipment_type eq, int sub_type) const;

    virtual int hunger_level() const { return HS_ENGORGED; }
    virtual void make_hungry(int nutrition, bool silent = true)
    {
    }

    // Need not be implemented for the player - player action costs
    // are explicitly calculated.
    virtual void lose_energy(energy_use_type)
    {
    }
    
    virtual std::string name(description_level_type type) const = 0;
    virtual std::string pronoun(pronoun_type which_pronoun) const = 0;
    virtual std::string conj_verb(const std::string &verb) const = 0;

    virtual bool fumbles_attack(bool verbose = true) = 0;

    // Returns true if the actor has no way to attack (plants, statues).
    // (statues have only indirect attacks).
    virtual bool cannot_fight() const = 0;
    virtual void attacking(actor *other) = 0;
    virtual bool can_go_berserk() const = 0;
    virtual bool can_see_invisible() const = 0;
    virtual bool invisible() const = 0;
    virtual bool visible_to(const actor *looker) const = 0;
    virtual bool can_see(const actor *target) const = 0;
    virtual bool is_icy() const = 0;
    virtual void go_berserk(bool intentional) = 0;
    virtual void mutate() = 0;
    virtual int hurt(const actor *attacker, int amount) = 0;
    virtual void heal(int amount, bool max_too = false) = 0;
    virtual void banish(const std::string &who = "") = 0;
    virtual void blink() = 0;
    virtual void teleport(bool right_now = false, bool abyss_shift = false) = 0;
    virtual void poison(actor *attacker, int amount = 1) = 0;
    virtual bool sicken(int amount) = 0;
    virtual void paralyse(int strength) = 0;
    virtual void slow_down(int strength) = 0;
    virtual void confuse(int strength) = 0;
    virtual void rot(actor *attacker, int rotlevel, int immediate_rot) = 0;
    virtual void expose_to_element(beam_type element, int strength = 0) = 0;
    virtual void drain_stat(int stat, int amount, actor* attacker) { }
    virtual void put_to_sleep(int power = 0) { };
    virtual void check_awaken(int disturbance) = 0;

    virtual bool wearing_light_armour(bool = false) const { return (true); }
    virtual int  skill(skill_type sk, bool skill_bump = false) const
    {
        return (0);
    }
    
    virtual void exercise(skill_type sk, int qty) { }

    virtual int stat_hp() const = 0;
    virtual int stat_maxhp() const = 0;
    virtual int armour_class() const = 0;
    virtual int melee_evasion(const actor *attacker) const = 0;
    virtual int shield_bonus() const = 0;
    virtual int shield_block_penalty() const = 0;
    virtual int shield_bypass_ability(int tohit) const = 0;

    virtual void shield_block_succeeded() { }

    virtual int mons_species() const = 0;
    
    virtual mon_holy_type holiness() const = 0;
    virtual int res_fire() const = 0;
    virtual int res_steam() const = 0;
    virtual int res_cold() const = 0;
    virtual int res_elec() const = 0;
    virtual int res_poison() const = 0;
    virtual int res_negative_energy() const = 0;

    virtual flight_type flight_mode() const = 0;
    virtual bool is_levitating() const = 0;
    virtual bool airborne() const;
    
    virtual bool paralysed() const = 0;
    virtual bool confused() const = 0;
    virtual bool caught() const = 0;
    virtual bool asleep() const { return (false); }
    virtual bool backlit() const = 0;

    virtual void god_conduct(conduct_type thing_done, int level) { }

    virtual bool incapacitated() const
    {
        return paralysed() || asleep() || confused() || caught();
    }

    virtual int holy_aura() const
    {
        return (0);
    }

    virtual int warding() const
    {
        return (0);
    }

    virtual bool visible() const
    {
        return (true);
    }

    virtual bool has_spell(spell_type spell) const = 0;

    virtual bool     will_trigger_shaft() const;
    virtual level_id shaft_dest() const;
    virtual bool     do_shaft() = 0;
};

struct coord_def
{
    int         x;
    int         y;

    explicit coord_def( int x_in = 0, int y_in = 0 ) : x(x_in), y(y_in) { }

    void set(int xi, int yi)
    {
        x = xi;
        y = yi;
    }

    void reset()
    {
        set(0, 0);
    }

    int distance_from(const coord_def &b) const;
    
    bool operator == (const coord_def &other) const
    {
        return x == other.x && y == other.y;
    }

    bool operator != (const coord_def &other) const
    {
        return !operator == (other);
    }

    bool operator <  (const coord_def &other) const
    {
        return (x < other.x) || (x == other.x && y < other.y);
    }

    const coord_def &operator += (const coord_def &other)
    {
        x += other.x;
        y += other.y;
        return (*this);
    }

    const coord_def &operator += (int offset)
    {
        x += offset;
        y += offset;
        return (*this);
    }
    
    const coord_def &operator -= (const coord_def &other)
    {
        x -= other.x;
        y -= other.y;
        return (*this);
    }

    const coord_def &operator -= (int offset)
    {
        x -= offset;
        y -= offset;
        return (*this);
    }

    const coord_def &operator /= (int div)
    {
        x /= div;
        y /= div;
        return (*this);
    }

    const coord_def &operator *= (int mul)
    {
        x *= mul;
        y *= mul;
        return (*this);
    }
    
    coord_def operator + (const coord_def &other) const
    {
        coord_def copy = *this;
        return (copy += other);
    }

    coord_def operator + (int other) const
    {
        coord_def copy = *this;
        return (copy += other);
    }

    coord_def operator - (const coord_def &other) const
    {
        coord_def copy = *this;
        return (copy -= other);
    }

    coord_def operator - (int other) const
    {
        coord_def copy = *this;
        return (copy -= other);
    }

    coord_def operator / (int div) const
    {
        coord_def copy = *this;
        return (copy /= div);
    }

    coord_def operator * (int mul) const
    {
        coord_def copy = *this;
        return (copy *= mul);
    }
    
    int abs() const
    {
        return (x * x + y * y);
    }

    int rdist() const
    {
        return (std::max(std::abs(x), std::abs(y)));
    }

    bool origin() const
    {
        return (!x && !y);
    }
};
typedef bool (*coord_predicate)(const coord_def &c);

struct dice_def
{
    int         num;
    int         size;

    dice_def( int n = 0, int s = 0 ) : num(n), size(s) {}
};

struct run_check_dir
{
    dungeon_feature_type grid;
    int dx;
    int dy;
};


struct delay_queue_item
{
    delay_type  type;
    int         duration;
    int         parm1;
    int         parm2;
    bool        started;
};


struct item_def 
{
    object_class_type  base_type;  // basic class (ie OBJ_WEAPON)
    unsigned char  sub_type;   // type within that class (ie WPN_DAGGER)
    short          plus;       // +to hit, charges, corpse mon id
    short          plus2;      // +to dam, sub-sub type for boots and helms
    long           special;    // special stuff
    unsigned char  colour;     // item colour
    unsigned long  flags;      // item status flags
    short          quantity;   // number of items

    short  x;          // x-location;         for inventory items = -1 
    short  y;          // y-location;         for inventory items = -1
    short  link;       // link to next item;  for inventory items = slot
    short  slot;       // Inventory letter

    unsigned short orig_place;
    short          orig_monnum;

    std::string inscription;

    CrawlHashTable props;

public:
    item_def() : base_type(OBJ_UNASSIGNED), sub_type(0), plus(0), plus2(0),
                 special(0L), colour(0), flags(0L), quantity(0),
                 x(0), y(0), link(NON_ITEM), slot(0), orig_place(0),
                 orig_monnum(0), inscription()
    {
    }

    std::string name(description_level_type descrip,
                     bool terse = false, bool ident = false,
                     bool with_inscription = true,
                     bool quantity_in_words = false,
                     unsigned long ignore_flags = 0x0) const;
    bool has_spells() const;
    bool cursed() const;
    int  book_number() const;
    int zap() const; // what kind of beam it shoots (if wand).
                     // XXX should really return zap_type!
    

    // Returns index in mitm array. Results are undefined if this item is
    // not in the array!
    int  index() const;

    int  armour_rating() const;
    
    bool launched_by(const item_def &launcher) const;

    void clear()
    {
        *this = item_def();
    }
private:
    std::string name_aux( description_level_type desc,
                          bool terse, bool ident,
                          unsigned long ignore_flags ) const;
};

class runrest
{
public:
    int runmode;
    int mp;
    int hp;
    int x, y;

    FixedVector<run_check_dir,3> run_check; // array of grids to check

public:
    runrest();
    void initialise(int rdir, int mode);

    operator int () const;
    const runrest &operator = (int newrunmode);

    // Returns true if we're currently resting.
    bool is_rest() const;

    bool is_explore() const;

    // Clears run state.
    void clear();

    // Stops running.
    void stop();

    // Take one off the rest counter.
    void rest();

    // Checks if shift-run should be aborted and aborts the run if necessary.
    // Returns true if you were running and are now no longer running.
    bool check_stop_running();

    // Check if we've reached the HP/MP stop-rest condition
    void check_hp();
    void check_mp();

private:
    void set_run_check(int index, int compass_dir);
    bool run_grids_changed() const;
};

class PlaceInfo
{
public:
    int level_type;
    int branch;

    unsigned long num_visits;
    unsigned long levels_seen;

    unsigned long mon_kill_exp;
    unsigned long mon_kill_exp_avail;
    unsigned long mon_kill_num[KC_NCATEGORIES];

    long turns_total;
    long turns_explore;
    long turns_travel;
    long turns_interlevel;
    long turns_resting;
    long turns_other;

    double elapsed_total;
    double elapsed_explore;
    double elapsed_travel;
    double elapsed_interlevel;
    double elapsed_resting;
    double elapsed_other;

public:
    PlaceInfo();

    bool is_global() const;
    void make_global();

    void assert_validity() const;

    const std::string short_name() const;

    const PlaceInfo &operator += (const PlaceInfo &other);
    const PlaceInfo &operator -= (const PlaceInfo &other);
    PlaceInfo operator + (const PlaceInfo &other) const;
    PlaceInfo operator - (const PlaceInfo &other) const;
};


typedef std::vector<delay_queue_item> delay_queue_type;

class KillMaster;



class player : public actor
{
public:
  bool turn_is_over; // flag signaling that player has performed a timed action
  
  // If true, player is headed to the Abyss.
  bool banished;
  std::string banished_by;

  std::vector<int> beheld_by; // monsters beholding player

  bool just_autoprayed;         // autopray just kicked in
    
  unsigned short prev_targ;
  coord_def     prev_grd_targ;
  char your_name[kNameLen];

  species_type species;
  job_type char_class;

  // Coordinates of last travel target; note that this is never used by
  // travel itself, only by the level-map to remember the last travel target.
  short travel_x, travel_y;

  runrest running;            // Nonzero if running/traveling.

  char special_wield;

  double elapsed_time;        // total amount of elapsed time in the game

  unsigned char synch_time;   // amount to wait before calling handle_time

  unsigned char disease;

  char max_level;

  int x_pos;
  int y_pos;

  int prev_move_x;
  int prev_move_y;

  int hunger;
  FixedVector<char, NUM_EQUIP> equip;

  int hp;
  int hp_max;
  int base_hp;                // temporary max HP loss (rotting)
  int base_hp2;               // base HPs from levels (and permanent loss)

  int magic_points;
  int max_magic_points;
  int base_magic_points;      // temporary max MP loss? (currently unused)
  int base_magic_points2;     // base MPs from levels and potions of magic

  char strength;
  char intel;
  char dex;
  char max_strength;
  char max_intel;
  char max_dex;

  char hunger_state;

  bool wield_change;          // redraw weapon
  bool quiver_change;         // redraw quiver
  bool received_weapon_warning;

  unsigned long redraw_status_flags;

  // PC's symbol (usually @) and colour.
  int symbol;
  int colour;

  bool redraw_hit_points;
  bool redraw_magic_points;
  bool redraw_strength;
  bool redraw_intelligence;
  bool redraw_dexterity;
  bool redraw_experience;
  bool redraw_armour_class;

  bool redraw_gold;
  bool redraw_evasion;

  unsigned char flash_colour;

  unsigned char hit_points_regeneration;
  unsigned char magic_points_regeneration;

  unsigned long experience;
  int experience_level;
  int gold;
  char class_name[30];
  int time_taken;

  char shield_blocks;         // number of shield blocks since last action

  FixedVector< item_def, ENDOFPACK > inv;

  int burden;
  burden_state_type burden_state;
  FixedVector<spell_type, 25> spells;
  char spell_no;
  game_direction_type char_direction;

  unsigned short pet_target;

  int your_level; // offset by one (-1 == 0, 0 == 1, etc.) for display

  // durational things
  FixedVector<int, NUM_DURATIONS> duration; 

  int rotting;

  int berserk_penalty;                // penalty for moving while berserk

  FixedVector<unsigned char, NUM_ATTRIBUTES> attribute;
  FixedVector<unsigned char, NUM_QUIVER> quiver; // default items for quiver
  FixedVector<long, NUM_OBJECT_CLASSES> sacrifice_value;

  undead_state_type is_undead;

  delay_queue_type delay_queue;       // pending actions

  FixedVector<unsigned char, 50>  skills;
  FixedVector<bool, 50>  practise_skill;
  FixedVector<unsigned int, 50>   skill_points;
  FixedVector<unsigned char, 50>  skill_order;

  skill_type sage_bonus_skill;  // if Sage is in effect, which skill it affects
  int sage_bonus_degree;        // how much bonus XP to give in that skill

  int  skill_cost_level;
  int  total_skill_points;
  int  exp_available;

  FixedArray<unsigned char, 6, 50> item_description;
  FixedVector<unique_item_status_type, 50> unique_items;
  FixedVector<bool, NUM_MONSTERS> unique_creatures;

  // NOTE: The kills member is a pointer to a KillMaster object,
  // rather than the object itself, so that we can get away with
  // just a foward declare of the KillMaster class, rather than
  // having to #include Kills.h and thus make every single .cc file
  // dependant on Kills.h.  Having a pointer means that we have
  // to do our own implementations of copying the player object,
  // since the default implementations will lead to the kills member
  // pointing to freed memory, or worse yet lead to the same piece of
  // memory being freed twice.
  KillMaster* kills;

  level_area_type level_type;
  std::string level_type_name;

  entry_cause_type entry_cause;
  god_type         entry_cause_god;

  branch_type where_are_you;

  FixedVector<unsigned char, 30> branch_stairs;

  god_type religion;
  unsigned char piety;
  unsigned char piety_hysteresis;       // amount of stored-up docking
  unsigned char gift_timeout;
  FixedVector<unsigned char, MAX_NUM_GODS>  penance;
  FixedVector<unsigned char, MAX_NUM_GODS>  worshipped;
  FixedVector<short,         MAX_NUM_GODS>  num_gifts;


  FixedVector<unsigned char, NUM_MUTATIONS> mutation;
  FixedVector<unsigned char, NUM_MUTATIONS> demon_pow;
  unsigned char magic_contamination;

  FixedVector<bool, NUM_BOOKS> had_book;

  unsigned char normal_vision;        // how far the species gets to see
  unsigned char current_vision;       // current sight radius (cells)

  unsigned char hell_exit;            // which level plyr goes to on hell exit.

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

protected:
  FixedVector<PlaceInfo, NUM_BRANCHES>             branch_info;
  FixedVector<PlaceInfo, NUM_LEVEL_AREA_TYPES - 1> non_branch_info;

public:
    player();
    player(const player &other);
    ~player();

    void copy_from(const player &other);

    void init();
    
    // Low-level move the player to (x, y). Use these functions instead of
    // changing x_pos and y_pos directly.
    void moveto(int x, int y);
    void moveto(const coord_def &c);

    coord_def prev_move() const;
    void reset_prev_move();
    
    bool in_water() const;
    bool can_swim() const;
    bool is_levitating() const;
    bool cannot_speak() const;
    bool invisible() const;
    bool can_see_invisible() const;
    bool visible_to(const actor *looker) const;
    bool can_see(const actor *target) const;
    bool is_icy() const;

    bool light_flight() const;
    bool travelling_light() const;

    kill_category kill_alignment() const;

    bool has_spell(spell_type spell) const;

    size_type transform_size(int psize = PSIZE_TORSO) const;
    std::string shout_verb() const;

    item_def *slot_item(equipment_type eq);

    // actor
    int id() const;
    int       get_experience_level() const;
    actor_type atype() const { return ACT_PLAYER; }

    god_type  deity() const;
    bool      alive() const;

    coord_def pos() const;
    bool      swimming() const;
    bool      submerged() const;
    bool      floundering() const;
    bool      can_pass_through_feat(dungeon_feature_type grid) const;
    size_type body_size(int psize = PSIZE_TORSO, bool base = false) const;
    int       body_weight() const;
    int       total_weight() const;
    int       damage_type(int attk = -1);
    int       damage_brand(int attk = -1);
    int       has_claws(bool allow_tran = true) const;
    bool      has_usable_claws(bool allow_tran = true) const;
    item_def *weapon(int which_attack = -1);
    item_def *shield();

    std::string name(description_level_type type) const;
    std::string pronoun(pronoun_type pro) const;
    std::string conj_verb(const std::string &verb) const;

    bool fumbles_attack(bool verbose = true);
    bool cannot_fight() const;

    void attacking(actor *other);
    bool can_go_berserk() const;
    bool can_go_berserk(bool verbose) const;
    void go_berserk(bool intentional);
    void mutate();
    void banish(const std::string &who = "");
    void blink();
    void teleport(bool right_now = false, bool abyss_shift = false);
    void drain_stat(int stat, int amount, actor* attacker);

    void expose_to_element(beam_type element, int strength = 0);
    void god_conduct(conduct_type thing_done, int level);

    int hunger_level() const { return hunger_state; }
    void make_hungry(int nutrition, bool silent = true);
    void poison(actor *agent, int amount = 1);
    bool sicken(int amount);
    void paralyse(int str);
    void slow_down(int str);
    void confuse(int strength);
    void rot(actor *agent, int rotlevel, int immed_rot);
    void heal(int amount, bool max_too = false);
    int hurt(const actor *agent, int amount);

    int holy_aura() const;
    int warding() const;

    int mons_species() const;

    mon_holy_type holiness() const;
    int res_fire() const;
    int res_steam() const;
    int res_cold() const;
    int res_elec() const;
    int res_poison() const;
    int res_negative_energy() const;
    bool confusable() const;
    bool slowable() const;

    bool omnivorous() const;

    flight_type flight_mode() const;
    bool permanent_levitation() const;
    bool permanent_flight() const;

    bool paralysed() const;
    bool confused() const;
    bool caught() const;
    bool backlit() const;

    bool asleep() const;
    void put_to_sleep(int power = 0);
    void awake();
    void check_awaken(int disturbance);

    bool cannot_act() const;

    bool can_throw_rocks() const;

    int armour_class() const;
    int melee_evasion(const actor *attacker) const;

    int stat_hp() const     { return hp; }
    int stat_maxhp() const  { return hp_max; }
    
    int shield_bonus() const;
    int shield_block_penalty() const;
    int shield_bypass_ability(int tohit) const;

    void shield_block_succeeded();

    bool wearing_light_armour(bool with_skill = false) const;
    void exercise(skill_type skill, int qty);
    int  skill(skill_type skill, bool skill_bump = false) const;

    PlaceInfo& get_place_info() const ; // Current place info
    PlaceInfo& get_place_info(branch_type branch,
                              level_area_type level_type2) const;
    PlaceInfo& get_place_info(branch_type branch) const;
    PlaceInfo& get_place_info(level_area_type level_type2) const;

    void set_place_info(PlaceInfo info);
    // Returns copies of the PlaceInfo; modifying the vector won't
    // modify the player object.
    std::vector<PlaceInfo> get_all_place_info(bool visited_only = false,
                                              bool dungeon_only = false) const;

    bool do_shaft();
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
    player_save_info operator=(const player& rhs);
    bool operator<(const player_save_info& rhs) const;
    std::string short_desc() const;
};

class monster_spells : public FixedVector<spell_type, NUM_MONSTER_SPELL_SLOTS>
{
public:
    monster_spells()
        : FixedVector<spell_type, NUM_MONSTER_SPELL_SLOTS>(SPELL_NO_SPELL)
    { }
    void clear() { init(SPELL_NO_SPELL); }
};

class ghost_demon;

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

    void set_duration(const monsters *mons, const mon_enchant *exist);

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
    int modded_speed(const monsters *mons, int hdplus) const;
    int calc_duration(const monsters *mons, const mon_enchant *added) const;    
};

typedef std::map<enchant_type, mon_enchant> mon_enchant_list;

struct monsterentry;

class monsters : public actor
{
public:
    monsters();
    monsters(const monsters &other);
    ~monsters();

    monsters &operator = (const monsters &other);

public:
    int type;
    int hit_points;
    int max_hit_points;
    int hit_dice;
    int ac;
    int ev;
    int speed;
    int speed_increment;
    unsigned char x;
    unsigned char y;
    unsigned char target_x;
    unsigned char target_y;
    FixedVector<short, NUM_MONSTER_SLOTS> inv;
    monster_spells spells;
    mon_attitude_type attitude;
    beh_type behaviour;
    unsigned int foe;
    char ench_countdown;
    mon_enchant_list enchantments;
    unsigned long flags;               // bitfield of boolean flags

    unsigned long experience;
    unsigned int  number;              // #heads (hydra), etc.
    int           colour;

    int foe_memory;                    // how long to 'remember' foe x,y
                                       // once they go out of sight.

    int shield_blocks;                 // Count of shield blocks this round.

    god_type god;                      // Usually GOD_NO_GOD.

    std::auto_ptr<ghost_demon> ghost;  // Ghost information.

    std::string seen_context;          // Non-standard context for
                                       // AI_SEE_MONSTER

public:
    void init_experience();

    void mark_summoned(int longevity, bool mark_items_summoned );

    bool has_action_energy() const;
    void check_redraw(const coord_def &oldpos) const;
    void apply_location_effects();

    kill_category kill_alignment() const;

    int  foe_distance() const;
    bool needs_berserk(bool check_spells = true) const;

    bool has_ench(enchant_type ench) const;
    bool has_ench(enchant_type ench, enchant_type ench2) const;
    mon_enchant get_ench(enchant_type ench,
                         enchant_type ench2 = ENCH_NONE) const;
    bool add_ench(const mon_enchant &);
    void update_ench(const mon_enchant &);
    bool del_ench(enchant_type ench, bool quiet = false, bool effect = true);
    bool lose_ench_duration(const mon_enchant &e, int levels);
    bool lose_ench_levels(const mon_enchant &e, int lev);
    void lose_energy(energy_use_type et);

    void scale_hp(int num, int den);
    bool gain_exp(int exp);

    void react_to_damage(int damage);

    void add_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void remove_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void apply_enchantments();
    void apply_enchantment(const mon_enchant &me);

    void timeout_enchantments(int levels);
    
    bool needs_transit() const;
    void set_transit(const level_id &destination);
    bool find_place_to_live(bool near_player = false);
    bool find_place_near_player();
    bool find_home_around(const coord_def &c, int radius);
    bool find_home_anywhere();
        
    void set_ghost(const ghost_demon &ghost);
    void ghost_init();
    void pandemon_init();
    void destroy_inventory();
    void reset();
    void load_spells(mon_spellbook_type spellbook);

    actor *get_foe() const;
    
    // actor interface
    int id() const;
    int       get_experience_level() const;    
    god_type  deity() const;
    bool      alive() const;
    coord_def pos() const;
    coord_def target_pos() const;
    bool      swimming() const;
    bool      submerged() const;
    bool      can_drown() const;
    bool      floundering() const;
    bool      can_pass_through_feat(dungeon_feature_type grid) const;
    size_type body_size(int psize = PSIZE_TORSO, bool base = false) const;
    int       body_weight() const;
    int       total_weight() const;
    int       damage_type(int attk = -1);
    int       damage_brand(int attk = -1);

    item_def *slot_item(equipment_type eq);
    item_def *mslot_item(mon_inv_type sl) const;
    item_def *weapon(int which_attack = -1);
    item_def *launcher();
    item_def *missiles();
    item_def *shield();

    int       missile_count();
    void      wield_melee_weapon(int near = -1);
    void      swap_weapons(int near = -1);

    bool      pickup_item(item_def &item, int near = -1, bool force = false);
    void      pickup_message(const item_def &item, int near);
    bool      pickup_wand(item_def &item, int near);
    bool      pickup_scroll(item_def &item, int near);
    bool      pickup_potion(item_def &item, int near);
    bool      pickup_gold(item_def &item, int near);
    bool      pickup_launcher(item_def &launcher, int near);
    bool      pickup_melee_weapon(item_def &item, int near);
    bool      pickup_throwable_weapon(item_def &item, int near);
    bool      pickup_weapon(item_def &item, int near, bool force);
    bool      pickup_armour(item_def &item, int near, bool force);
    bool      pickup_misc(item_def &item, int near);
    bool      pickup_missile(item_def &item, int near, bool force);
    bool      eat_corpse(item_def &carrion, int near);
    void      equip(item_def &item, int slot, int near = -1);
    bool      unequip(item_def &item, int slot, int near = -1,
                      bool force = false);

    bool      can_use_missile(const item_def &item) const;
    
    std::string name(description_level_type type) const;
    std::string name(description_level_type type, bool force_visible) const;
    std::string pronoun(pronoun_type pro) const;
    std::string conj_verb(const std::string &verb) const;

    bool fumbles_attack(bool verbose = true);
    bool cannot_fight() const;

    int  skill(skill_type skill, bool skill_bump = false) const;

    void attacking(actor *other);
    bool can_go_berserk() const;
    void go_berserk(bool intentional);
    void mutate();
    void banish(const std::string &who = "");
    void expose_to_element(beam_type element, int strength = 0);
    bool visible() const;

    int mons_species() const;

    mon_holy_type holiness() const;
    int res_fire() const;
    int res_steam() const;    
    int res_cold() const;
    int res_elec() const;
    int res_poison() const;
    int res_negative_energy() const;

    flight_type flight_mode() const;
    bool is_levitating() const;
    bool invisible() const;
    bool can_see_invisible() const;
    bool visible_to(const actor *looker) const ;
    bool can_see(const actor *target) const;
    bool is_icy() const;
    bool paralysed() const;
    bool confused() const;
    bool caught() const;
    bool asleep() const;
    bool backlit() const;

    int holy_aura() const;

    bool has_spell(spell_type spell) const;

    int armour_class() const;
    int melee_evasion(const actor *attacker) const;

    void poison(actor *agent, int amount = 1);
    bool sicken(int strength);
    void paralyse(int str);
    void slow_down(int str);
    void confuse(int strength);
    void rot(actor *agent, int rotlevel, int immed_rot);
    int hurt(const actor *agent, int amount);
    void heal(int amount, bool max_too = false);
    void blink();
    void teleport(bool right_now = false, bool abyss_shift = false);

    void put_to_sleep(int power = 0);
    void check_awaken(int disturbance);

    int stat_hp() const    { return hit_points; }
    int stat_maxhp() const { return max_hit_points; }

    int shield_bonus() const;
    int shield_block_penalty() const;
    void shield_block_succeeded();
    int shield_bypass_ability(int tohit) const;

    actor_type atype() const { return ACT_MONSTER; }

    // Hacks, with a capital H.
    void fix_speed();
    void check_speed();
    void upgrade_type(monster_type after, bool adjust_hd, bool adjust_hp);

    std::string describe_enchantments() const;

    int action_energy(energy_use_type et) const;
    static int base_speed(int mcls);

    bool do_shaft();

private:
    void init_with(const monsters &mons);
    void swap_slots(mon_inv_type a, mon_inv_type b);
    bool need_message(int &near) const;
    bool level_up();
    bool level_up_change();
    bool pickup(item_def &item, int slot, int near, bool force_merge = false);
    void equip_weapon(item_def &item, int near);
    void equip_armour(item_def &item, int near);
    void unequip_weapon(item_def &item, int near);
    void unequip_armour(item_def &item, int near);
   
    bool decay_enchantment(const mon_enchant &me, bool decay_degree = true);
    
    bool drop_item(int eslot, int near);
    bool wants_weapon(const item_def &item) const;
    bool wants_armour(const item_def &item) const;
    bool can_throw_rocks() const;
    void lose_pickup_energy();
    bool check_set_valid_home(const coord_def &place,
                              coord_def &chosen,
                              int &nvalid) const;
    bool has_spell_of_type(unsigned) const;
    const monsterentry *find_monsterentry() const;
};

struct cloud_struct
{
    int           x;
    int           y;
    cloud_type    type;
    int           decay;
    unsigned char spread_rate;
    kill_category whose;

    killer_type beam_thrower() const;
};

struct shop_struct
{
    unsigned char       x;
    unsigned char       y;
    unsigned char       greed;
    shop_type           type;
    unsigned char       level;

    FixedVector<unsigned char, 3> keeper_name;
};

struct trap_struct
{
    unsigned char       x;
    unsigned char       y;
    trap_type        type;
};

struct map_cell
{
    short object;           // The object: monster, item, feature, or cloud.
    unsigned short flags;   // Flags describing the mappedness of this square.
    unsigned short colour;
    unsigned short property;

    map_cell() : object(0), flags(0), colour(0), property(0) { }
    void clear() { flags = object = colour = property = 0; }

    unsigned glyph() const;
    bool known() const;
    bool seen() const;
};

class map_marker;
class reader;
class writer;
class map_markers
{
public:
    map_markers();
    map_markers(const map_markers &);
    map_markers &operator = (const map_markers &);
    ~map_markers();

    void activate_all(bool verbose = true);
    void add(map_marker *marker);
    void remove(map_marker *marker);
    void remove_markers_at(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(map_marker_type type);
    void move(const coord_def &from, const coord_def &to);
    void move_marker(map_marker *marker, const coord_def &to);
    std::vector<map_marker*> get_all(map_marker_type type = MAT_ANY);
    std::vector<map_marker*> get_all(const std::string &key,
                                     const std::string &val = "");
    std::vector<map_marker*> get_markers_at(const coord_def &c);
    std::string property_at(const coord_def &c, map_marker_type type,
                            const std::string &key);
    void clear();

    void write(writer &) const;
    void read(reader &);

private:
    typedef std::multimap<coord_def, map_marker *> dgn_marker_map;
    typedef std::pair<coord_def, map_marker *> dgn_pos_marker;

    void init_from(const map_markers &);
    void unlink_marker(const map_marker *);

private:
    dgn_marker_map markers;
};

typedef FixedArray<dungeon_feature_type, GXM, GYM> feature_grid;
typedef FixedArray<unsigned, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER>
    env_show_grid;

struct crawl_environment
{
public:
    unsigned char rock_colour;
    unsigned char floor_colour;

    FixedVector< item_def, MAX_ITEMS >       item;  // item list
    FixedVector< monsters, MAX_MONSTERS >    mons;  // monster list

    feature_grid                             grid;  // terrain grid
    FixedArray< unsigned short, GXM, GYM >   mgrid; // monster grid
    FixedArray< int, GXM, GYM >              igrid; // item grid
    FixedArray< unsigned short, GXM, GYM >   cgrid; // cloud grid
    FixedArray< unsigned short, GXM, GYM >   grid_colours; // colour overrides

    FixedArray< map_cell, GXM, GYM >         map;    // discovered terrain

    // Glyphs of squares that are in LOS.
    env_show_grid show;
    
    // What would be visible, if all of the translucent wall were
    // made opaque.
    env_show_grid no_trans_show;
    
    FixedArray<unsigned short, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER>
                                             show_col;  // view window colour

#ifdef USE_TILE
    // indexed by grid coords
    FixedArray<unsigned int,GXM, GYM> tile_bk_fg; // tile fg
    FixedArray<unsigned int,GXM, GYM> tile_bk_bg; // tile bg
    FixedArray<TileFlavor, GXM, GYM> tile_flavor;
    // indexed by (show-1) coords
    FixedArray<unsigned int,ENV_SHOW_DIAMETER-2,ENV_SHOW_DIAMETER-2> tile_fg;
    FixedArray<unsigned int,ENV_SHOW_DIAMETER-2,ENV_SHOW_DIAMETER-2> tile_bg;
#endif

    FixedVector< cloud_struct, MAX_CLOUDS >  cloud; // cloud list
    unsigned char cloud_no;

    FixedVector< shop_struct, MAX_SHOPS >    shop;  // shop list
    FixedVector< trap_struct, MAX_TRAPS >    trap;  // trap list

    FixedVector< int, 20 >                   mons_alloc;
    map_markers                              markers;
    
    double                   elapsed_time; // used during level load

    // Number of turns the player has spent on this level.
    int turns_on_level;

    // Flags for things like preventing teleport control; see
    // level_flag_type in enum.h
    unsigned long level_flags;

    coord_def sanctuary_pos;
    int sanctuary_time;
};

extern struct crawl_environment env;

struct message_filter
{
    int             channel;        // Use -1 to match any channel.
    text_pattern    pattern;        // Empty pattern matches any message

    message_filter( int ch, const std::string &s ) 
        : channel(ch), pattern(s) 
    {
    }

    message_filter( const std::string &s ) : channel(-1), pattern(s) { }

    bool is_filtered( int ch, const std::string &s ) const {
        bool channel_match = ch == channel || channel == -1;
        if (!channel_match || pattern.empty())
            return channel_match;
        return pattern.matches(s);
    }

};

struct sound_mapping
{
    text_pattern pattern;
    std::string  soundfile;
};

struct colour_mapping
{
    std::string tag;
    text_pattern pattern;
    int colour;
};

struct message_colour_mapping
{
    message_filter message;
    int colour;
};

struct feature_def
{
    dungeon_char_type   dchar;
    unsigned            symbol;          // symbol used for seen terrain
    unsigned            magic_symbol;    // symbol used for magic-mapped terrain
    unsigned short      colour;          // normal in LoS colour
    unsigned short      map_colour;      // colour when out of LoS on display
    unsigned short      seen_colour;     // map_colour when is_terrain_seen()
    unsigned short      em_colour;       // Emphasised colour when in LoS.
    unsigned short      seen_em_colour;  // Emphasised colour when out of LoS
    unsigned            flags;

    bool is_notable() const { return (flags & FFT_NOTABLE); }
};

struct feature_override
{
    dungeon_feature_type    feat;
    feature_def             override;
};

class InvEntry;
typedef int (*item_sort_fn)(const InvEntry *a, const InvEntry *b);
struct item_comparator
{
    item_sort_fn cmpfn;
    bool negated;

    item_comparator(item_sort_fn cfn, bool neg = false)
        : cmpfn(cfn), negated(neg)
    {
    }
    int compare(const InvEntry *a, const InvEntry *b) const
    {
        return (negated? -cmpfn(a, b) : cmpfn(a, b));
    }
};
typedef std::vector<item_comparator> item_sort_comparators;

struct menu_sort_condition
{
public:
    menu_type mtype;
    int       sort;
    item_sort_comparators cmp;

public:
    menu_sort_condition(menu_type mt = MT_INVLIST, int sort = 0);
    menu_sort_condition(const std::string &s);

    bool matches(menu_type mt) const;

private:
    void set_menu_type(std::string &s);
    void set_sort(std::string &s);
    void set_comparators(std::string &s);
};

struct mon_display
{
    monster_type type;
    unsigned     glyph;
    unsigned     colour;

    mon_display(monster_type m = MONS_PROGRAM_BUG,
                unsigned gly = 0,
                unsigned col = 0) : type(m), glyph(gly), colour(col) { }
};

class InitLineInput;
struct game_options 
{
public:
    game_options();
    void reset_startup_options();
    void reset_options();

    void read_option_line(const std::string &s, bool runscripts = false);
    void read_options(InitLineInput &, bool runscripts,
                      bool clear_aliases = true);

public:
    // View options
    std::vector<feature_override> feature_overrides;
    std::vector<mon_display>      mon_glyph_overrides;
    unsigned cset_override[NUM_CSET][NUM_DCHAR_TYPES];

    std::string save_dir;       // Directory where saves and bones go.
    std::string macro_dir;      // Directory containing macro.txt
    std::string morgue_dir;     // Directory where character dumps and morgue
                                // dumps are saved. Overrides crawl_dir.
    std::vector<std::string> additional_macro_files;

    std::string player_name;

#ifdef DGL_SIMPLE_MESSAGING
    bool        messaging;      // Check for messages.
#endif

    bool        suppress_startup_errors;

    bool        mouse_input;
    
    int         view_max_width;
    int         view_max_height;
    int         mlist_min_height;
    bool        mlist_force_inline;

    // The view lock variables force centering the viewport around the PC @
    // at all times (the default). If view locking is not enabled, the viewport
    // scrolls only when the PC hits the edge of it.
    bool        view_lock_x;
    bool        view_lock_y;

    // For an unlocked viewport, this will center the viewport when scrolling.
    bool        center_on_scroll;

    // If symmetric_scroll is set, for diagonal moves, if the view
    // scrolls at all, it'll scroll diagonally.
    bool        symmetric_scroll;

    // How far from the viewport edge is scrolling forced.
    int         scroll_margin_x;
    int         scroll_margin_y;

    bool        autopickup_on;
    bool        autoprayer_on;
    bool        show_more_prompt;

    bool        show_turns;      // Show turns used in HUD.
    bool        show_beam;       // Show targeting beam by default.

    long        autopickups;     // items to autopickup
    bool        show_inventory_weights; // show weights in inventory listings
    bool        colour_map;      // add colour to the map
    bool        clean_map;       // remove unseen clouds/monsters
    bool        show_uncursed;   // label known uncursed items as "uncursed"
    bool        easy_open;       // open doors with movement
    bool        easy_unequip;    // allow auto-removing of armour / jewellery
    bool        easy_butcher;    // autoswap to butchering tool
    bool        always_confirm_butcher; // even if only one corpse
    bool        list_rotten;     // list slots for rotting corpses/chunks
    bool        default_target;  // start targeting on a real target
    bool        autopickup_no_burden;   // don't autopickup if it changes burden
    
    bool        note_all_skill_levels;  // take note for all skill levels (1-27)
    bool        note_skill_max;  // take note when skills reach new max
    bool        note_all_spells; // take note when learning any spell
    std::string user_note_prefix;// Prefix for user notes
    int         note_hp_percent; // percentage hp for notetaking
    int         ood_interesting; // how many levels OOD is noteworthy?
    int         easy_confirm;    // make yesno() confirming easier
    int         easy_quit_item_prompts; // make item prompts quitable on space
    int         colour[16];      // macro fg colours to other colours
    int         background;      // select default background colour
    int         channels[NUM_MESSAGE_CHANNELS];  // msg channel colouring
    
    bool        use_old_selection_order; // use old order of species/classes in
                                         // selection screen
    int         weapon;          // auto-choose weapon for character
    int         book;            // auto-choose book for character
    int         chaos_knight;    // choice of god for Chaos Knights (Xom/Makleb)
    int         death_knight;    // choice of god/necromancy for Death Knights
    god_type    priest;          // choice of god for priests (Zin/Yred)
    bool        random_pick;     // randomly generate character
    int         hp_warning;      // percentage hp for danger warning
    int         magic_point_warning;    // percentage mp for danger warning
    char        race;            // preselected race
    char        cls;             // preselected class
    bool        delay_message_clear;    // avoid clearing messages each turn
    unsigned    friend_brand;    // Attribute for branding friendly monsters
    unsigned    neutral_brand;   // Attribute for branding neutral monsters
    bool        no_dark_brand;   // Attribute for branding friendly monsters
    bool        macro_meta_entry;// Allow user to use numeric sequences when
                                 // creating macros

    int         fire_items_start;// index of first item for fire command
    std::vector<unsigned> fire_order;   // missile search order for 'f' command
    bool        fire_quiver_best;

    bool        auto_list;       // automatically jump to appropriate item lists

    bool        flush_input[NUM_FLUSH_REASONS]; // when to flush input buff

    char_set_type  char_set;
    FixedVector<unsigned, NUM_DCHAR_TYPES> char_table;

    int         num_colours;     // used for setting up curses colour table (8 or 16)
    
#ifdef WIZARD
    int         wiz_mode;        // yes, no, never in wiz mode to start
#endif

    // internal use only:
    int         sc_entries;      // # of score entries
    int         sc_format;       // Format for score entries

    std::vector<std::pair<int, int> > hp_colour;
    std::vector<std::pair<int, int> > mp_colour;

    std::string map_file_name;   // name of mapping file to use
    std::vector<text_pattern> never_pickup;   // Objects we'll never pick up
    std::vector<text_pattern> always_pickup;  // Stuff we always pick up
    std::vector<text_pattern> note_monsters;  // Interesting monsters
    std::vector<text_pattern> note_messages;  // Interesting messages
    std::vector<std::pair<text_pattern, std::string> > autoinscriptions;
    std::vector<text_pattern> note_items;     // Objects to note
    std::vector<int> note_skill_levels;       // Skill levels to note

    bool        pickup_thrown;  // Pickup thrown missiles
    bool        pickup_dropped; // Pickup dropped objects
    int         travel_delay;   // How long to pause between travel moves

    // Messages that stop travel
    std::vector<message_filter> travel_stop_message;

    int         stash_tracking; // How stashes are tracked

    int         tc_reachable;   // Colour for squares that are reachable
    int         tc_excluded;    // Colour for excluded squares.
    int         tc_exclude_circle; // Colour for squares in the exclusion radius
    int         tc_dangerous;   // Colour for trapped squares, deep water, lava.
    int         tc_disconnected;// Areas that are completely disconnected.

    int         travel_stair_cost;

    bool        show_waypoints;

    bool        classic_item_colours;   // Use old-style item colours
    bool        item_colour;    // Colour items on level map

    unsigned    detected_monster_colour;    // Colour of detected monsters
    unsigned    detected_item_colour;       // Colour of detected items
    unsigned    status_caption_colour;      // Colour of captions in HUD.

    unsigned    heap_brand;         // Highlight heaps of items
    unsigned    stab_brand;         // Highlight monsters that are stabbable
    unsigned    may_stab_brand;     // Highlight potential stab candidates
    unsigned    feature_item_brand; // Highlight features covered by items.
    unsigned    trap_item_brand;    // Highlight traps covered by items.
    
    bool        trap_prompt;        // Prompt when stepping on mechnical traps
                                    // without enough hp (using trapwalk.lua)

    // What is the minimum number of items in a stack for which
    // you show summary (one-line) information
    int         item_stack_summary_minimum;

    int         explore_stop;      // Stop exploring if a previously unseen
                                   // item comes into view

    int         explore_stop_prompt;

    bool        explore_greedy;    // Explore goes after items as well.

    // How much more eager greedy-explore is for items than to explore.
    int         explore_item_greed;

    // Some experimental improvments to explore
    bool        explore_improved;
    
    std::vector<sound_mapping> sound_mappings;
    std::vector<colour_mapping> menu_colour_mappings;
    std::vector<message_colour_mapping> message_colour_mappings;

    bool       menu_colour_prefix_class; // Prefix item class to string
    bool       menu_colour_prefix_id;    // Prefix id-status to string

    std::vector<menu_sort_condition> sort_menus;

    int         dump_kill_places;   // How to dump place information for kills.
    int         dump_message_count; // How many old messages to dump

    int         dump_item_origins;  // Show where items came from?
    int         dump_item_origin_price;

    // Order of sections in the character dump.
    std::vector<std::string> dump_order;

    bool        level_map_title;    // Show title in level map
    bool        target_zero_exp;    // If true, targeting targets zero-exp
                                    // monsters.
    bool        target_wrap;        // Wrap around from last to first target
    bool        target_oos;         // 'x' look around can target out-of-LOS
    bool        target_los_first;   // 'x' look around first goes to visible
                                    // objects/features, then goes to stuff
                                    // outside LOS.
    bool        target_unshifted_dirs; // Unshifted keys target if cursor is
                                       // on player.

    int         drop_mode;          // Controls whether single or multidrop
                                    // is the default.
    int         pickup_mode;        // -1 for single, 0 for menu,
                                    // X for 'if at least X items present'

    bool        easy_exit_menu;     // Menus are easier to get out of

    int         assign_item_slot;   // How free slots are assigned

    std::vector<text_pattern> drop_filter;
    
    FixedArray<bool, NUM_DELAYS, NUM_AINTERRUPTS> activity_interrupts;

    // Previous startup options
    bool        remember_name;      // Remember and reprompt with last name

    bool        dos_use_background_intensity;
    
    bool        use_fake_cursor;    // Draw a fake cursor instead of relying
                                    // on the term's own cursor.

    int         level_map_cursor_step;  // The cursor increment in the level
                                        // map.

    // If the player prefers to merge kill records, this option can do that.
    int         kill_map[KC_NCATEGORIES];

#ifdef WIZARD
    // Parameters for fight simulations.
    long        fsim_rounds;
    int         fsim_str, fsim_int, fsim_dex;
    int         fsim_xl;
    std::string fsim_mons;
    std::vector<std::string> fsim_kit;
#endif  // WIZARD
    
#ifdef USE_TILE
    char        tile_show_items[20]; // show which item types in tile inventory
    bool        tile_title_screen;   // display title screen?
    // minimap colours
    char        tile_player_col;
    char        tile_monster_col;
    char        tile_neutral_col;
    char        tile_friendly_col;
    char        tile_item_col;
    char        tile_unseen_col;
    char        tile_floor_col;
    char        tile_wall_col;
    char        tile_mapped_wall_col;
    char        tile_door_col;
    char        tile_downstairs_col;
    char        tile_upstairs_col;
    char        tile_feature_col;
    char        tile_trap_col;
    char        tile_water_col;
    char        tile_lava_col;
    char        tile_excluded_col;
    char        tile_excl_centre_col;
#endif
#ifdef WIN32TILES
    bool        use_dos_char;
#endif
    
    typedef std::map<std::string, std::string> opt_map;
    opt_map     named_options;          // All options not caught above are
                                        // recorded here.
        
    ///////////////////////////////////////////////////////////////////////
    // These options cannot be directly set by the user. Instead they're
    // set indirectly to the choices the user made for the last character
    // created. XXX: Isn't there a better place for these?
    std::string prev_name;
    char        prev_race;
    char        prev_cls;
    int         prev_ck, prev_dk;
    god_type    prev_pr;
    int         prev_weapon;
    int         prev_book;
    bool        prev_randpick;

    ///////////////////////////////////////////////////////////////////////
    // tutorial
    FixedVector<bool, 48> tutorial_events;
//    bool tut_made_note;
    bool tut_explored;
    bool tut_stashes;
    bool tut_travel;
    unsigned int tut_spell_counter;
    unsigned int tut_throw_counter;
    unsigned int tut_berserk_counter;
    unsigned int tut_melee_counter;
    unsigned int tut_last_healed;
    
    bool tut_just_triggered;
    unsigned int tutorial_type;
    unsigned int tutorial_left;

private:
    std::map<std::string, std::string> aliases;
    
public:
    // Convenience accessors for the second-class options in named_options.
    int         o_int(const char *name, int def = 0) const;
    long        o_long(const char *name, long def = 0L) const;
    bool        o_bool(const char *name, bool def = false) const;
    std::string o_str(const char *name, const char *def = NULL) const;
    int         o_colour(const char *name, int def = LIGHTGREY) const;

    // Fix option values if necessary, specifically file paths.
    void fixup_options();

private:
    std::string unalias(const std::string &key) const;
    void add_alias(const std::string &alias, const std::string &name);
    
    void clear_feature_overrides();
    void clear_cset_overrides();
    void add_cset_override(char_set_type set, const std::string &overrides);
    void add_cset_override(char_set_type set, dungeon_char_type dc,
                           unsigned symbol);
    void add_feature_override(const std::string &);

    void add_message_colour_mappings(const std::string &);
    void add_message_colour_mapping(const std::string &);
    message_filter parse_message_filter(const std::string &s);
    
    void set_default_activity_interrupts();
    void clear_activity_interrupts(FixedVector<bool, NUM_AINTERRUPTS> &eints);
    void set_activity_interrupt(
        FixedVector<bool, NUM_AINTERRUPTS> &eints,
        const std::string &interrupt);
    void set_activity_interrupt(const std::string &activity_name,
                                const std::string &interrupt_names,
                                bool append_interrupts,
                                bool remove_interrupts);
    void set_fire_order(const std::string &full, bool add);
    void add_fire_order_slot(const std::string &s);
    void set_menu_sort(std::string field);
    void new_dump_fields(const std::string &text, bool add = true);
    void do_kill_map(const std::string &from, const std::string &to);
    int  read_explore_stop_conditions(const std::string &) const;

    void split_parse(const std::string &s, const std::string &separator,
                     void (game_options::*add)(const std::string &));
    void add_mon_glyph_override(monster_type mtype, mon_display &mdisp);    
    void add_mon_glyph_overrides(const std::string &mons, mon_display &mdisp);
    void add_mon_glyph_override(const std::string &);
    mon_display parse_mon_glyph(const std::string &s) const;
    void set_option_fragment(const std::string &s);    

    static const std::string interrupt_prefix;
};

extern game_options  Options;

#if _MSC_VER
# include "msvc.h"
#endif

#endif // EXTERNS_H
