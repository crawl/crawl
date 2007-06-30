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

#include <time.h>

#include "defines.h"
#include "enum.h"
#include "FixAry.h"
#include "Kills.h"
#include "libutil.h"
#include "message.h"

#define INFO_SIZE       200          // size of message buffers
#define ITEMNAME_SIZE   200          // size of item names/shop names/etc
#define HIGHSCORE_SIZE  800          // <= 10 Lines for long format scores

#define MAX_NUM_GODS    21

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

struct monsters;
struct ait_hp_loss;

struct activity_interrupt_data
{
    activity_interrupt_payload_type apt;
    const void *data;
    std::string context;

    activity_interrupt_data()
        : apt(AIP_NONE), data(NULL), context()
    {
    }
    activity_interrupt_data(const int *i)
        : apt(AIP_INT), data(i), context()
    {
    }
    activity_interrupt_data(const char *s)
        : apt(AIP_STRING), data(s), context()
    {
    }
    activity_interrupt_data(const std::string &s) 
        : apt(AIP_STRING), data(s.c_str()), context()
    {
    }
    activity_interrupt_data(const monsters *m, const std::string &ctx = "")
        : apt(AIP_MONSTER), data(m), context(ctx)
    {
    }
    activity_interrupt_data(const ait_hp_loss *ahl)
        : apt(AIP_HP_LOSS), data(ahl), context()
    {
    }
    activity_interrupt_data(const activity_interrupt_data &a)
        : apt(a.apt), data(a.data), context(a.context)
    {
    }
};

class item_def;
class melee_attack;
class coord_def;

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
    
    virtual size_type body_size(int psize = PSIZE_TORSO,
                                bool base = false) const = 0;
    
    virtual int       damage_type(int which_attack = -1) = 0;
    virtual int       damage_brand(int which_attack = -1) = 0;
    virtual item_def *weapon(int which_attack = -1) = 0;
    virtual item_def *shield() = 0;
    virtual item_def *slot_item(equipment_type eq) = 0;

    virtual int hunger_level() const { return HS_ENGORGED; }
    virtual void make_hungry(int nutrition, bool silent = true)
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
    virtual bool is_icy() const = 0;
    virtual bool invisible() const = 0;
    virtual void go_berserk(bool intentional) = 0;
    virtual void mutate() = 0;
    virtual void hurt(const actor *attacker, int amount) = 0;
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
    virtual void drain_stat(int stat, int amount) { }

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
    virtual int res_cold() const = 0;
    virtual int res_elec() const = 0;
    virtual int res_poison() const = 0;
    virtual int res_negative_energy() const = 0;

    virtual int levitates() const = 0;
    
    virtual bool paralysed() const = 0;
    virtual bool confused() const = 0;
    virtual bool asleep() const { return (false); }
    virtual bool backlit() const = 0;

    virtual void god_conduct(conduct_type thing_done, int level) { }

    virtual bool incapacitated() const
    {
        return paralysed() || confused();
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
};

struct ait_hp_loss
{
    int hp;
    int hurt_type;  // KILLED_BY_POISON, etc.

    ait_hp_loss(int _hp, int _ht) : hp(_hp), hurt_type(_ht) { }
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

    int abs() const
    {
        return (x * x + y * y);
    }

    int rdist() const
    {
        return (MAXIMUM(::abs(x), ::abs(y)));
    }
};
typedef bool (*coord_predicate)(const coord_def &c);

struct dice_def
{
    int         num;
    int         size;

    dice_def( int n = 0, int s = 0 ) : num(n), size(s) {}
};

struct ray_def
{
public:
    double accx;
    double accy;
    double slope;
    // Quadrant 1: down-right
    // Quadrant 2: down-left
    // Quadrant 3: up-left
    // Quadrant 4: up-right
    int quadrant;
    int fullray_idx;            // for cycling: where did we come from?

public:
    int x() const { return static_cast<int>(accx); }
    int y() const { return static_cast<int>(accy); }
    coord_def pos() const { return coord_def(x(), y()); }
    
    // returns the direction taken (0,1,2)
    int advance(bool shorten = false, const coord_def *p = NULL);
    int advance_through(const coord_def &point);
    void advance_and_bounce();
    void regress(const coord_def &point);

private:
    int raw_advance();
    double reflect(bool x, double oldc, double newc) const;
    double reflect(double x, double c) const;
    void set_reflect_point(const double oldx, const double oldy,
                           double *newx, double *newy,
                           bool blocked_x, bool blocked_y);
};

// output from direction() function:
struct dist
{
    bool isValid;       // valid target chosen?
    bool isTarget;      // target (true), or direction (false)?
    bool isMe;          // selected self (convenience: tx == you.x_pos,
                        // ty == you.y_pos)
    bool isEndpoint;    // Does the player want the attack to stop at (tx,ty)?
    bool isCancel;      // user cancelled (usually <ESC> key)
    bool choseRay;      // user wants a specific beam
    int  tx,ty;         // target x,y or logical extension of beam to map edge
    int  dx,dy;         // delta x and y if direction - always -1,0,1
    ray_def ray;        // ray chosen if necessary

    // internal use - ignore
    int  prev_target;   // previous target

    // target - source (source == you.pos())
    coord_def target() const
    {
        return coord_def(tx, ty);
    }
};

struct bolt
{
    // INPUT parameters set by caller
    int         range;                 // minimum range
    int         rangeMax;              // maximum range
    int         type;                  // missile gfx
    int         colour;
    int         flavour;
    int         source_x, source_y;    // beam origin
    dice_def    damage;
    int         ench_power, hit;
    int         target_x, target_y;    // intended target
    char        thrower;               // what kind of thing threw this?
    char        ex_size;               // explosion radius (0==none)
    int         beam_source;           // NON_MONSTER or monster index #
    std::string name;
    bool        is_beam;               // beams? (can hits multiple targets?)
    bool        is_explosion;
    bool        is_big_cloud;          // expands into big_cloud at endpoint
    bool        is_enchant;            // no block/dodge, but mag resist
    bool        is_energy;             // mostly energy/non-physical attack
    bool        is_launched;           // was fired from launcher?
    bool        is_thrown;             // was thrown from hand?
    bool        target_first;          // targeting by direction 
    bool        aimed_at_spot;         // aimed at (x,y), should not cross
    std::string aux_source;            // source of KILL_MISC beams

    // OUTPUT parameters (tracing, ID)
    bool        obvious_effect;         // did an 'obvious' effect happen?
    int         fr_count, foe_count;   // # of times a friend/foe is "hit"
    int         fr_power, foe_power;   // total levels/hit dice affected

    // INTERNAL use - should not usually be set outside of beam.cc
    bool        is_tracer;      // is this a tracer?
    bool        aimed_at_feet;   // this was aimed at self!
    bool        msg_generated;  // an appropriate msg was already mpr'd
    bool        in_explosion_phase;   // explosion phase (as opposed to beam phase)
    bool        smart_monster;  // tracer firer can guess at other mons. resists?
    bool        can_see_invis;   // tracer firer can see invisible?
    bool        is_friendly;    // tracer firer is enslaved or pet
    int         foe_ratio;      // 100* foe ratio (see mons_should_fire())
    bool        chose_ray;      // do we want a specific ray?
    ray_def     ray;            // shoot on this specific ray

public:
    // A constructor to try and fix some of the bugs that occur because
    // this struct never seems to be properly initialized.  Definition
    // is over in beam.cc.
    bolt();

    void set_target(const dist &);

    // Returns YOU_KILL or MON_KILL, depending on the source of the beam.
    int  killer() const;

    coord_def target() const
    {
        return (coord_def(target_x, target_y));
    }
};

struct run_check_dir
{
    unsigned char       grid;
    char                dx;
    char                dy;
};


struct delay_queue_item
{
    int  type;
    int  duration;
    int  parm1;
    int  parm2;
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

public:
    item_def() : base_type(OBJ_UNASSIGNED), sub_type(0), plus(0), plus2(0),
                 special(0L), colour(0), flags(0L), quantity(0),
                 x(0), y(0), link(NON_ITEM), slot(0), orig_place(0),
                 orig_monnum(0), inscription()
    {
    }

    std::string name(description_level_type descrip,
                     bool terse = false, bool ident = false,
                     bool with_inscription = true ) const;
    bool has_spells() const;
    bool cursed() const;
    int  book_number() const;

    void clear()
    {
        *this = item_def();
    }
private:
    std::string name_aux( description_level_type desc,
                          bool terse, bool ident ) const;
};

class input_history
{
public:
    input_history(size_t size);

    void new_input(const std::string &s);
    void clear();
    
    const std::string *prev();
    const std::string *next();

    void go_end();
private:
    typedef std::list<std::string> string_list;
    
    string_list             history;
    string_list::iterator   pos;
    size_t maxsize;
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

typedef std::vector<delay_queue_item> delay_queue_type;

class player : public actor
{
public:
  bool turn_is_over; // flag signaling that player has performed a timed action

  // If true, player is headed to the Abyss.
  bool banished;
  std::string banished_by;

  bool just_autoprayed;         // autopray just kicked in
    
  unsigned char prev_targ;
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
  game_direction_type char_direction;          //

  unsigned char pet_target;

  int your_level; // offset by one (-1 == 0, 0 == 1, etc.) for display

  // durational things
  FixedVector<int, NUM_DURATIONS> duration; 

  int rotting;

  int berserk_penalty;                // penalty for moving while berserk

  FixedVector<unsigned char, NUM_ATTRIBUTES> attribute;

  undead_state_type is_undead;

  delay_queue_type delay_queue;       // pending actions

  FixedVector<unsigned char, 50>  skills;
  FixedVector<bool, 50>  practise_skill;
  FixedVector<unsigned int, 50>   skill_points;
  FixedVector<unsigned char, 50>  skill_order;
  int  skill_cost_level;
  int  total_skill_points;
  int  exp_available;

  FixedArray<unsigned char, 5, 50> item_description;
  FixedVector<unsigned char, 50> unique_items;
  FixedVector<bool, NUM_MONSTERS> unique_creatures;

  KillMaster kills;

  level_area_type level_type;

  branch_type where_are_you;

  FixedVector<unsigned char, 30> branch_stairs;

  god_type religion;
  unsigned char piety;
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

public:
    player();
    void init();
    
    bool is_valid() const;
    std::string short_desc() const;
    
    // For sorting
    bool operator < (const player &p) const;

public:
    // Low-level move the player to (x, y). Use these functions instead of
    // changing x_pos and y_pos directly.
    void moveto(int x, int y);
    void moveto(const coord_def &c);
    
    bool in_water() const;
    bool can_swim() const;
    bool is_levitating() const;
    bool cannot_speak() const;
    bool invisible() const;
    bool can_see_invisible() const;
    bool is_icy() const;

    kill_category kill_alignment() const;

    bool has_spell(int spell) const;

    size_type transform_size(int psize = PSIZE_TORSO) const;
    std::string shout_verb() const;

    item_def *slot_item(equipment_type eq);

    // actor
    int id() const;
    actor_type atype() const { return ACT_PLAYER; }

    god_type  deity() const;
    bool      alive() const;
    
    coord_def pos() const;
    bool      swimming() const;
    bool      submerged() const;
    bool      floundering() const;
    size_type body_size(int psize = PSIZE_TORSO, bool base = false) const;
    int       damage_type(int attk = -1);
    int       damage_brand(int attk = -1);
    bool      has_usable_claws() const;
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
    void drain_stat(int stat, int amount);

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
    void hurt(const actor *agent, int amount);

    int holy_aura() const;
    int warding() const;

    int mons_species() const;

    mon_holy_type holiness() const;
    int res_fire() const;
    int res_cold() const;
    int res_elec() const;
    int res_poison() const;
    int res_negative_energy() const;

    int levitates() const;

    bool paralysed() const;
    bool confused() const;
    bool backlit() const;

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
};

extern player you;

class monster_spells : public FixedVector<spell_type, NUM_MONSTER_SPELL_SLOTS>
{
public:
    monster_spells() 
        : FixedVector<spell_type, NUM_MONSTER_SPELL_SLOTS>(SPELL_NO_SPELL)
    { }
    void clear() { init(SPELL_NO_SPELL); }
};

struct mon_attack_def
{
    mon_attack_type     type;
    mon_attack_flavour  flavour;
    int                 damage;

    static mon_attack_def attk(int damage,
                               mon_attack_type type = AT_HIT,
                               mon_attack_flavour flav = AF_PLAIN)
    {
        mon_attack_def def = { type, flav, damage };
        return (def);
    }
};

class ghost_demon;
class level_id;

struct mon_enchant
{
    enchant_type  ench;
    int           degree;
    kill_category who;      // Who set this enchantment?

    mon_enchant(enchant_type e = ENCH_NONE, int deg = 0,
                kill_category whose = KC_OTHER)
        : ench(e), degree(deg), who(whose)
    {
    }

    int killer() const;
    int kill_agent() const;
    
    operator std::string () const;
    const char *kill_category_desc(kill_category) const;
    void merge_killer(kill_category who);
    void cap_degree();

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
};

typedef std::set<mon_enchant> mon_enchant_list;

class monsters : public actor
{
public:
    monsters();
    monsters(const monsters &other);

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
    FixedVector<int, 8> inv;
    monster_spells spells;
    mon_attitude_type attitude;
    beh_type behaviour;
    unsigned int foe;
    mon_enchant_list enchantments;
    unsigned long flags;               // bitfield of boolean flags

    unsigned int number;               // #heads (hydra), etc.
    int          colour;

    int foe_memory;                    // how long to 'remember' foe x,y
                                       // once they go out of sight

    god_type god;                      // Usually GOD_NO_GOD.

    std::auto_ptr<ghost_demon> ghost;  // Ghost information.

public:
    kill_category kill_alignment() const;

    int  foe_distance() const;
    bool needs_berserk(bool check_spells = true) const;
    
    bool has_ench(enchant_type ench) const;
    bool has_ench(enchant_type ench, enchant_type ench2) const;
    mon_enchant get_ench(enchant_type ench,
                         enchant_type ench2 = ENCH_NONE) const;
    bool add_ench(const mon_enchant &);
    void update_ench(const mon_enchant &);
    bool del_ench(enchant_type ench, bool quiet = false);

    void scale_hp(int num, int den);

    void lose_ench_levels(const mon_enchant &e, int levels);
    void add_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void remove_enchantment_effect(const mon_enchant &me, bool quiet = false);
    void apply_enchantments(int speed);
    void apply_enchantment(mon_enchant me, int speed);

    void timeout_enchantments(int levels);
    
    bool needs_transit() const;
    void set_transit(const level_id &destination);
    bool find_place_to_live(bool near_player = false);
    bool find_place_near_player();
    bool find_home_in(coord_def s, coord_def e);
    bool find_home_anywhere();
        
    void set_ghost(const ghost_demon &ghost);
    void ghost_init();
    void pandemon_init();
    void destroy_inventory();
    void reset();
    void load_spells(int spellbook);

    actor *get_foe() const;
    
    // actor interface
    int id() const;
    god_type  deity() const;
    bool      alive() const;
    coord_def pos() const;
    bool      swimming() const;
    bool      submerged() const;
    bool      can_drown() const;
    bool      floundering() const;
    size_type body_size(int psize = PSIZE_TORSO, bool base = false) const;
    int       damage_type(int attk = -1);
    int       damage_brand(int attk = -1);

    item_def *slot_item(equipment_type eq);
    item_def *weapon(int which_attack = -1);
    item_def *shield();
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
    int res_cold() const;
    int res_elec() const;
    int res_poison() const;
    int res_negative_energy() const;

    int levitates() const;
    bool invisible() const;
    bool can_see_invisible() const;
    bool is_icy() const;
    bool paralysed() const;
    bool confused() const;
    bool asleep() const;
    bool backlit() const;

    int holy_aura() const;

    int armour_class() const;
    int melee_evasion(const actor *attacker) const;

    void poison(actor *agent, int amount = 1);
    bool sicken(int strength);
    void paralyse(int str);
    void slow_down(int str);
    void confuse(int strength);
    void rot(actor *agent, int rotlevel, int immed_rot);
    void hurt(const actor *agent, int amount);
    void heal(int amount, bool max_too = false);
    void blink();
    void teleport(bool right_now = false, bool abyss_shift = false);

    int stat_hp() const    { return hit_points; }
    int stat_maxhp() const { return max_hit_points; }
    
    int shield_bonus() const;
    int shield_block_penalty() const;
    int shield_bypass_ability(int tohit) const;

    actor_type atype() const { return ACT_MONSTER; }

    // Hack, with a capital H.
    void fix_speed();
    void check_speed();

    static int base_speed(int mcls);

private:
    void init_with(const monsters &mons);
};

struct cloud_struct
{
    int           x;
    int           y;
    cloud_type    type;
    int           decay;
    kill_category whose;
};

struct shop_struct
{
    unsigned char       x;
    unsigned char       y;
    unsigned char       greed;
    unsigned char       type;
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

    map_cell() : object(0), flags(0), colour(0) { }
    void clear() { flags = object = colour = 0; }

    unsigned glyph() const;
    bool known() const;
    bool seen() const;
};

typedef FixedArray<dungeon_feature_type, GXM, GYM> feature_grid;
class map_marker;
typedef std::multimap<coord_def, map_marker *> dgn_marker_map;
typedef std::pair<coord_def, map_marker *> dgn_pos_marker;

struct crawl_environment
{
public:
    unsigned char rock_colour;
    unsigned char floor_colour;

    FixedVector< item_def, MAX_ITEMS >       item;  // item list
    FixedVector< monsters, MAX_MONSTERS >    mons;  // monster list

    feature_grid                             grid;  // terrain grid
    FixedArray< unsigned char, GXM, GYM >    mgrid; // monster grid
    FixedArray< int, GXM, GYM >              igrid; // item grid
    FixedArray< unsigned char, GXM, GYM >    cgrid; // cloud grid

    FixedArray< map_cell, GXM, GYM >        map;    // discovered terrain

    FixedArray<unsigned, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER>
                                             show;      // view window char 
    FixedArray<unsigned short, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER>
                                             show_col;  // view window colour

    FixedVector< cloud_struct, MAX_CLOUDS >  cloud; // cloud list
    unsigned char cloud_no;

    FixedVector< shop_struct, MAX_SHOPS >    shop;  // shop list
    FixedVector< trap_struct, MAX_TRAPS >    trap;  // trap list

    FixedVector< int, 20 >                   mons_alloc;
    dgn_marker_map                           markers;
    
    double                   elapsed_time; // used during level load

    // Number of turns the player has spent on this level.
    int turns_on_level;

public:
    void add_marker(map_marker *);
    void remove_marker(map_marker *);
    void remove_markers_at(const coord_def &c);
    map_marker *find_marker(const coord_def &c, map_marker_type) const;
    std::vector<map_marker*> get_markers(const coord_def &c) const;
    void clear_markers();
};

extern struct crawl_environment env;

// Track various aspects of Crawl game state.
struct game_state
{
    bool mouse_enabled;     // True if mouse input is currently relevant.
    
    bool waiting_for_command; // True when the game is waiting for a command.
    bool terminal_resized;   // True if the term was resized and we need to
                             // take action to handle it.
    
    bool io_inited;         // Is curses or the equivalent initialised?
    bool need_save;         // Set to true when game has started.
    bool saving_game;       // Set to true while in save_game.
    bool updating_scores;   // Set to true while updating hiscores.
    bool shopping;          // Set if id has been munged for shopping.

    int seen_hups;          // Set to true if SIGHUP received.

    bool unicode_ok;        // Is unicode support available?

    std::string (*glyph2strfn)(unsigned glyph);
    int  (*multibyte_strlen)(const std::string &s);
    void (*terminal_resize_handler)();
    void (*terminal_resize_check)();

    game_state() : mouse_enabled(false), waiting_for_command(false),
                   terminal_resized(false), io_inited(false), need_save(false),
                   saving_game(false), updating_scores(false), shopping(false),
                   seen_hups(0), unicode_ok(false), glyph2strfn(NULL),
                   multibyte_strlen(NULL), terminal_resize_handler(NULL),
                   terminal_resize_check(NULL)
    {
    }

    void check_term_size() const
    {
        if (terminal_resize_check)
            (*terminal_resize_check)();
    }
};
extern game_state crawl_state;

struct ghost_demon
{
public:
    std::string name;
    FixedVector< short, NUM_GHOST_VALUES > values;

public:
    ghost_demon();
    void reset();
    void init_random_demon();
    void init_player_ghost();

public:
    static std::vector<ghost_demon> find_ghosts();

private:
    static int n_extra_ghosts();
    static void find_extra_ghosts(std::vector<ghost_demon> &ghosts, int n);
    static void find_transiting_ghosts(std::vector<ghost_demon> &gs, int n);
    static void announce_ghost(const ghost_demon &g);
    
private:
    void add_spells();
    int  translate_spell(int playerspell) const;
};

extern std::vector<ghost_demon> ghosts;

struct system_environment
{
    std::string crawl_name;
    std::string crawl_pizza;
    std::string crawl_rc;
    std::string crawl_dir;
    std::string crawl_base;        // Directory from argv[0], may be used to
                                   // locate datafiles.
    std::string home;              // only used by MULTIUSER systems
    bool  board_with_nail;         // Easter Egg silliness

#ifdef DGL_SIMPLE_MESSAGING
    std::string messagefile;       // File containing messages from other users.
    bool have_messages;            // There are messages waiting to be read.
    unsigned  message_check_tick;
#endif

    std::string scorefile;
    std::vector<std::string> cmd_args;
};

extern system_environment SysEnv;

struct crawl_view_geometry
{
public:
    coord_def termsz;              // Size of the terminal.
    coord_def viewp;               // Left-top pos of viewport.
    coord_def viewsz;              // Size of the viewport (play area).
    coord_def hudp;                // Left-top pos of status area.
    coord_def hudsz;               // Size of the status area.
    coord_def msgp;                // Left-top pos of the message pane.
    coord_def msgsz;               // Size of the message pane.

    coord_def vgrdc;               // What grid pos is at the centre of the view
                                   // usually you.pos().

    coord_def viewhalfsz;

    coord_def glos1, glos2;        // LOS limit grid coords (inclusive)
    coord_def vlos1, vlos2;        // LOS limit viewport coords (inclusive)

    coord_def mousep;              // Where the mouse is.

    static const int message_min_lines = 7;
    static const int hud_min_width  = 41;
    static const int hud_min_gutter = 3;
    static const int hud_max_gutter = 6;

private:
    coord_def last_player_pos;
    
public:
    crawl_view_geometry();
    void init_geometry();

    void init_view();
    void set_player_at(const coord_def &c, bool force_centre = false);

    coord_def view_centre() const
    {
        return viewp + viewhalfsz;
    }

    bool in_grid_los(const coord_def &c) const
    {
        return (c.x >= glos1.x && c.x <= glos2.x
                && c.y >= glos1.y && c.y <= glos2.y);
    }

    bool in_view_los(const coord_def &c) const
    {
        return (c.x >= vlos1.x && c.x <= vlos2.x
                && c.y >= vlos1.y && c.y <= vlos2.y);
    }

    bool in_view_viewport(const coord_def &c) const
    {
        return (c.x >= viewp.x && c.y >= viewp.y
                && c.x < viewp.x + viewsz.x
                && c.y < viewp.y + viewsz.y);
    }

    bool in_grid_viewport(const coord_def &c) const
    {
        return in_view_viewport(c - vgrdc + view_centre());
    }
};

extern crawl_view_geometry crawl_view;

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
    unsigned            symbol;          // symbol used for seen terrain
    unsigned            magic_symbol;    // symbol used for magic-mapped terrain
    unsigned short      colour;          // normal in LoS colour
    unsigned short      map_colour;      // colour when out of LoS on display
    unsigned short      seen_colour;     // map_colour when is_terrain_seen()
    unsigned short      em_colour;       // Emphasised colour when in LoS.
    unsigned short      seen_em_colour;  // Emphasised colour when out of LoS
    bool                notable;         // gets noted when seen
    bool                seen_effect;     // requires special handling when seen
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

class InitLineInput;
struct game_options 
{
public:
    game_options();
    void reset_startup_options();
    void reset_options();

    void read_option_line(const std::string &s, bool runscripts = false);
    void read_options(InitLineInput &, bool runscripts);

public:
    // View options
    std::vector<feature_override> feature_overrides;
    unsigned cset_override[NUM_CSET][NUM_DCHAR_TYPES];

    std::string save_dir;       // Directory where saves and bones go.
    std::string macro_dir;      // Directory containing macro.txt
    std::string morgue_dir;     // Directory where character dumps and morgue
                                // dumps are saved. Overrides crawl_dir.

    std::vector<std::string> extra_levels;

    std::string player_name;

#ifdef DGL_SIMPLE_MESSAGING
    bool        messaging;      // Check for messages.
#endif

    bool        mouse_input;
    
    int         view_max_width;
    int         view_max_height;

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

    bool        show_turns;     // Show turns used in HUD.

    long        autopickups;    // items to autopickup
    bool        show_inventory_weights; // show weights in inventory listings
    bool        colour_map;     // add colour to the map
    bool        clean_map;      // remove unseen clouds/monsters
    bool        show_uncursed;  // label known uncursed items as "uncursed"
    bool        always_greet;   // display greeting message when reloading
    bool        easy_open;      // open doors with movement
    bool        easy_unequip;   // allow auto-removing of armour / jewelry
    bool        easy_butcher;   // open doors with movement
    bool        increasing_skill_progress; // skills go from 0-10 or 10-0
    bool        confirm_self_target; // require confirmation before selftarget
    bool        default_target;  // start targeting on a real target
    bool        safe_autopickup; // don't autopickup when monsters visible
    bool        autopickup_no_burden; // don't autopickup if it changes burden
    bool        note_skill_max; // take note when skills reach new max
    bool        note_all_spells; // take note when learning any spell
    bool        use_notes;      // take (and dump) notes
    int         note_hp_percent; // percentage hp for notetaking
    int         ood_interesting; // how many levels OOD is noteworthy?
    int         easy_confirm;   // make yesno() confirming easier
    int         easy_quit_item_prompts; // make item prompts quitable on space
    int         colour[16];     // macro fg colours to other colours
    int         background;     // select default background colour
    int         channels[NUM_MESSAGE_CHANNELS];  // msg channel colouring
    int         weapon;         // auto-choose weapon for character
    int         book;           // auto-choose book for character
    int         chaos_knight;   // choice of god for Chaos Knights (Xom/Makleb)
    int         death_knight;   // choice of god/necromancy for Death Knights
    god_type    priest;         // choice of god for priests (Zin/Yred)
    bool        random_pick;    // randomly generate character
    int         hp_warning;     // percentage hp for danger warning
    int         magic_point_warning;    // percentage mp for danger warning
    char        race;           // preselected race
    char        cls;            // preselected class
    bool        terse_hand;     // use terse description for wielded item
    bool        delay_message_clear; // avoid clearing messages each turn
    unsigned    friend_brand;   // Attribute for branding friendly monsters
    bool        no_dark_brand;  // Attribute for branding friendly monsters
    bool        macro_meta_entry; // Allow user to use numeric sequences when
                                  // creating macros

    int         fire_items_start; // index of first item for fire command
    FixedVector<fire_type, NUM_FIRE_TYPES> fire_order; // order for 'f' command

    bool        auto_list;      // automatically jump to appropriate item lists

    bool        flush_input[NUM_FLUSH_REASONS]; // when to flush input buff
    bool        lowercase_invocations;          // prefer lowercase invocations

    char_set_type  char_set;
    FixedVector<unsigned, NUM_DCHAR_TYPES> char_table;

    int         num_colours;    // used for setting up curses colour table (8 or 16)
    
#ifdef WIZARD
    int         wiz_mode;       // yes, no, never in wiz mode to start
#endif

    // internal use only:
    int         sc_entries;     // # of score entries
    int         sc_format;      // Format for score entries

    std::vector<std::pair<int, int> > hp_colour;
    std::vector<std::pair<int, int> > mp_colour;

    std::string map_file_name;  // name of mapping file to use
    std::vector<text_pattern> never_pickup;   // Objects we'll never pick up
    std::vector<text_pattern> always_pickup;  // Stuff we always pick up
    std::vector<text_pattern> note_monsters;  // Interesting monsters
    std::vector<text_pattern> note_messages;  // Interesting messages
    std::vector<std::pair<text_pattern, std::string> > autoinscriptions;
    std::vector<text_pattern> note_items; // Objects to note
    std::vector<int> note_skill_levels; // Skill levels to note

    bool        pickup_thrown;  // Pickup thrown missiles
    bool        pickup_dropped; // Pickup dropped objects
    int         travel_delay;   // How long to pause between travel moves

    // Messages that stop travel
    std::vector<message_filter> travel_stop_message;

    int         stash_tracking; // How stashes are tracked

    bool        travel_colour;  // Colour levelmap using travel information?
    int         tc_reachable;   // Colour for squares that are reachable
    int         tc_excluded;    // Colour for excluded squares.
    int         tc_exclude_circle; // Colour for squares in the exclusion radius
    int         tc_dangerous;   // Colour for trapped squares, deep water, lava.
    int         tc_disconnected;// Areas that are completely disconnected.

    int         travel_stair_cost;

    int         travel_exclude_radius2; // Square of the travel exclude radius
    bool        show_waypoints;

    bool        classic_item_colours;   // Use old-style item colours
    bool        item_colour;    // Colour items on level map

    unsigned    detected_monster_colour;    // Colour of detected monsters
    unsigned    detected_item_colour;       // Colour of detected items

    unsigned    heap_brand;       // Highlight heaps of items
    unsigned    stab_brand;       // Highlight monsters that are stabbable
    unsigned    may_stab_brand;   // Highlight potential stab candidates
    unsigned    stair_item_brand; // Highlight stairs covered by items.

    // What is the minimum number of items in a stack for which
    // you show summary (one-line) information
    int         item_stack_summary_minimum;

    int         explore_stop;      // Stop exploring if a previously unseen
                                   // item comes into view

    int         explore_stop_prompt;

    bool        explore_greedy;    // Explore goes after items as well.
    
    // How much more eager greedy-explore is for items than to explore.
    int         explore_item_greed;
    
    std::vector<sound_mapping> sound_mappings;
    std::vector<colour_mapping> menu_colour_mappings;
    std::vector<message_colour_mapping> message_colour_mappings;

    std::vector<menu_sort_condition> sort_menus;

    int         dump_kill_places; // How to dump place information for kills.
    int         dump_message_count; // How many old messages to dump

    int         dump_item_origins;  // Show where items came from?
    int         dump_item_origin_price;

    // Order of sections in the character dump.
    std::vector<std::string> dump_order;

    bool        level_map_title;    // Show title in level map
    bool        safe_zero_exp; // If true, you feel safe around 0xp monsters
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
    FixedVector<bool, 45> tutorial_events;
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
    void set_menu_sort(std::string field);
    void new_dump_fields(const std::string &text, bool add = true);
    void do_kill_map(const std::string &from, const std::string &to);
    int  read_explore_stop_conditions(const std::string &) const;
    void validate_options();

    static const std::string interrupt_prefix;
};

extern game_options  Options;

struct tagHeader 
{
    short tagID;
    long offset;
};

extern const struct coord_def Compass[8];
extern const char* god_gain_power_messages[MAX_NUM_GODS][MAX_GOD_ABILITIES];

typedef int keycode_type;

typedef FixedArray < item_type_id_state_type, 4, 50 > id_arr;

#endif // EXTERNS_H
