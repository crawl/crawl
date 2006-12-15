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

    activity_interrupt_data()
        : apt(AIP_NONE), data(NULL)
    {
    }
    activity_interrupt_data(const int *i)
        : apt(AIP_INT), data(i)
    {
    }
    activity_interrupt_data(const char *s)
        : apt(AIP_STRING), data(s)
    {
    }
    activity_interrupt_data(const std::string &s) 
        : apt(AIP_STRING), data(s.c_str())
    {
    }
    activity_interrupt_data(const monsters *m)
        : apt(AIP_MONSTER), data(m)
    {
    }
    activity_interrupt_data(const ait_hp_loss *ahl)
        : apt(AIP_HP_LOSS), data(ahl)
    {
    }
    activity_interrupt_data(const activity_interrupt_data &a)
        : apt(a.apt), data(a.data)
    {
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

    // coord_def( int x_in = 0, int y_in = 0 ) : x(x_in), y(y_in) {};
    bool operator == (const coord_def &other) const {
        return x == other.x && y == other.y;
    }

    bool operator != (const coord_def &other) const {
        return x != other.x || y != other.y;
    }
};

struct dice_def
{
    int         num;
    int         size;

    dice_def( int n = 0, int s = 0 ) : num(n), size(s) {}
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
    int  tx,ty;         // target x,y or logical extension of beam to map edge
    int  dx,dy;         // delta x and y if direction - always -1,0,1

    // internal use - ignore
    int  prev_target;   // previous target
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
    bool        is_beam;                // beams? (can hits multiple targets?)
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

public:
    // A constructor to try and fix some of the bugs that occur because
    // this struct never seems to be properly initialized.  Definition
    // is over in beam.cc.
    bolt();

    void set_target(const dist &);
};

struct ray_def
{
    double accx;
    double accy;
    double slope;
    // Quadrant 1: down-right
    // Quadrant 2: down-left
    // Quadrant 3: up-left
    // Quadrant 4: up-right
    int quadrant;
    
    int x() const { return (int)(accx); }
    int y() const { return (int)(accy); }
    int advance();              // returns the direction taken (0,1,2)
    void advance_and_bounce();
    void regress();
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
    unsigned char  base_type;  // basic class (ie OBJ_WEAPON)
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
    
    item_def() : base_type(OBJ_UNASSIGNED), sub_type(0), plus(0), plus2(0),
                 special(0L), colour(0), flags(0L), quantity(0),
                 x(0), y(0), link(NON_ITEM), slot(0), orig_place(0),
                 orig_monnum(0)
    {
        inscription.clear();
    }

    void clear()
    {
        *this = item_def();
    }
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

    // Clears run state.
    void clear();

    // Stops running.
    void stop();

    // Take one off the rest counter.
    void rest();

    // Decrements the run counter. Identical to rest.
    void rundown();

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

struct player
{
  bool turn_is_over; // flag signaling that player has performed a timed action

  unsigned char prev_targ;
  char your_name[kNameLen];

  unsigned char species;

  // Coordinates of last travel target; note that this is never used by
  // travel itself, only by the level-map to remember the last travel target.
  short travel_x, travel_y;

  runrest running;            // Nonzero if running/traveling.

  char special_wield;
  char deaths_door;
  char fire_shield;

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

  FixedVector< char, NUM_STATUE_TYPES >  visible_statue;

  char redraw_hit_points;
  char redraw_magic_points;
  char redraw_strength;
  char redraw_intelligence;
  char redraw_dexterity;
  char redraw_experience;
  char redraw_armour_class;

  char redraw_gold;
  char redraw_evasion;

  unsigned char flash_colour;

  unsigned char hit_points_regeneration;
  unsigned char magic_points_regeneration;

  unsigned long experience;
  int experience_level;
  unsigned int gold;
  int char_class;
  char class_name[30];
  // char speed;              // now unused
  int time_taken;

  char shield_blocks;         // number of shield blocks since last action

  FixedVector< item_def, ENDOFPACK > inv;

  int burden;
  char burden_state;
  FixedVector<unsigned char, 25> spells;
  char spell_no;
  unsigned char char_direction;          //

  unsigned char pet_target;

  int your_level; // offset by one (-1 == 0, 0 == 1, etc.) for display

  // durational things. Why didn't I do this for haste etc 
  // right from the start? Oh well.
  FixedVector<int, NUM_DURATIONS> duration; 

  int invis;
  int conf;
  int paralysis;
  int slow;
  int haste;
  int might;
  int levitation;

  int poison;
  int rotting;
  int berserker;

  int exhausted;                      // fatigue counter for berserk

  int berserk_penalty;                // pelnalty for moving while berserk

  FixedVector<unsigned char, 30> attribute;        // see ATTRIBUTES in enum.h

  char is_undead;                     // see UNDEAD_STATES in enum.h

  delay_queue_type delay_queue;       // pending actions

  FixedVector<unsigned char, 50>  skills;
  FixedVector<unsigned char, 50>  practise_skill;
  FixedVector<unsigned int, 50>   skill_points;
  FixedVector<unsigned char, 50>  skill_order;
  int  skill_cost_level;
  int  total_skill_points;
  int  exp_available;

  FixedArray<unsigned char, 5, 50> item_description;
  FixedVector<unsigned char, 50> unique_items;
  FixedVector<unsigned char, 50> unique_creatures;

  KillMaster kills;

  char level_type;

  char where_are_you;

  FixedVector<unsigned char, 30> branch_stairs;

  char religion;
  unsigned char piety;
  unsigned char gift_timeout;
  FixedVector<unsigned char, MAX_NUM_GODS>  penance;
  FixedVector<unsigned char, MAX_NUM_GODS>  worshipped;
  FixedVector<short,         MAX_NUM_GODS>  num_gifts;


  FixedVector<unsigned char, 100> mutation;
  FixedVector<unsigned char, 100> demon_pow;
  unsigned char magic_contamination;

  char confusing_touch;
  char sure_blade;

  FixedVector<unsigned char, 50> had_book;

  unsigned char betrayal;
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

  // Warning: these two are quite different.
  //
  // The spell table is an index to a specific spell slot (you.spells).
  // The ability table lists the ability (ABIL_*) which prefers that letter.
  //
  // In other words, the spell table contains hard links and the ability
  // table contains soft links.
  FixedVector<int, 52>  spell_letter_table;   // ref to spell by slot
  FixedVector<int, 52>  ability_letter_table; // ref to ability by enum

public:
  player();
  void init();

  bool is_valid() const;
  std::string short_desc() const;

  // For sorting
  bool operator < (const player &p) const;
};

extern struct player you;

class monster_spells : public FixedVector<int, NUM_MONSTER_SPELL_SLOTS>
{
public:
    monster_spells() 
        : FixedVector<int, NUM_MONSTER_SPELL_SLOTS>(MS_NO_SPELL)
    { }
    void clear() { init(MS_NO_SPELL); }
};

struct monsters
{
    int type;
    int hit_points;
    int max_hit_points;
    int hit_dice;
    int armour_class;       // great -- more mixed american/proper spelling
    int evasion;
    unsigned int speed;
    unsigned int speed_increment;
    unsigned char x;
    unsigned char y;
    unsigned char target_x;
    unsigned char target_y;
    FixedVector<int, 8> inv;
    monster_spells spells;
    unsigned char attitude;            // from MONS_ATTITUDE
    unsigned int behaviour;
    unsigned int foe;
    FixedVector<unsigned int, NUM_MON_ENCHANTS> enchantment;
    unsigned char flags;               // bitfield of boolean flags

    unsigned int number;               // #heads (hydra), etc.
    int          colour;

    int foe_memory;                    // how long to 'remember' foe x,y
                                       // once they go out of sight
};

struct cloud_struct
{
    unsigned char       x;
    unsigned char       y;
    unsigned char       type;
    int                 decay;
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
    unsigned char       type;
};

struct map_colour
{
    short colour;
    short flags;

    operator short () const { return colour; }
    void clear()            { colour = flags = 0; }
};

struct crawl_environment
{
    unsigned char rock_colour;
    unsigned char floor_colour;

    FixedVector< item_def, MAX_ITEMS >       item;  // item list
    FixedVector< monsters, MAX_MONSTERS >    mons;  // monster list

    FixedArray< unsigned char, GXM, GYM >    grid;  // terrain grid
    FixedArray< unsigned char, GXM, GYM >    mgrid; // monster grid
    FixedArray< int, GXM, GYM >              igrid; // item grid
    FixedArray< unsigned char, GXM, GYM >    cgrid; // cloud grid

    FixedArray< unsigned short, GXM, GYM >    map;    // discovered terrain
    FixedArray< map_colour, GXM, GYM >        map_col; // map colours

    FixedArray< unsigned int, 19, 19>        show;      // view window char 
    FixedArray< unsigned short, 19, 19>      show_col;  // view window colour

    FixedVector< cloud_struct, MAX_CLOUDS >  cloud; // cloud list
    unsigned char cloud_no;

    FixedVector< shop_struct, MAX_SHOPS >    shop;  // shop list
    FixedVector< trap_struct, MAX_TRAPS >    trap;  // trap list

    FixedVector< int, 20 >   mons_alloc;
    int                      trap_known;
    double                   elapsed_time; // used during level load
};

extern struct crawl_environment env;


struct ghost_struct
{
    char name[20];
    FixedVector< short, NUM_GHOST_VALUES > values;
};


extern struct ghost_struct ghost;

struct system_environment
{
    char *crawl_name;
    char *crawl_pizza;
    char *crawl_rc;
    char *crawl_dir;
    char *home;                 // only used by MULTIUSER systems
    bool  board_with_nail;      // Easter Egg silliness

    std::string scorefile;
};

extern system_environment SysEnv;

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
    std::string save_dir;       // Directory where saves and bones go.

    std::string player_name;

    bool        autopickup_on;
    bool        autoprayer_on;
    bool        fizzlecheck_on;

    long        autopickups;    // items to autopickup
    bool        verbose_dump;   // make character dumps contain more detail
    bool        detailed_stat_dump; // add detailed stat and resist dump
    bool        colour_map;     // add colour to the map
    bool        clean_map;      // remove unseen clouds/monsters
    bool        show_uncursed;  // label known uncursed items as "uncursed"
    bool        always_greet;   // display greeting message when reloading
    bool        easy_open;      // open doors with movement
    bool        easy_unequip;   // allow auto-removing of armour / jewelry
    bool        easy_butcher;   // open doors with movement
    bool        increasing_skill_progress; // skills go from 0-10 or 10-0
    bool        confirm_self_target; // require confirmation before selftarget
    bool        safe_autopickup; // don't autopickup when monsters visible
    bool        note_skill_max; // take note when skills reach new max
    bool        note_all_spells; // take note when learning any spell
    bool        use_notes;	// take (and dump) notes
    int         note_hp_percent; // percentage hp for notetaking
    int         ood_interesting; // how many levels OOD is noteworthy?
    int         easy_confirm;   // make yesno() confirming easier
    int         easy_quit_item_prompts; // make item prompts quitable on space
    int         colour[16];     // macro fg colours to other colours
    int         background;     // select default background colour
    int         channels[NUM_MESSAGE_CHANNELS];  // msg channel colouring
    int         weapon;         // auto-choose weapon for character
    int         book;		// auto-choose book for character
    int         chaos_knight;   // choice of god for Chaos Knights (Xom/Makleb)
    int         death_knight;   // choice of god/necromancy for Death Knights
    int         priest;         // choice of god for priests (Zin/Yred)
    bool        random_pick;    // randomly generate character
    int         hp_warning;     // percentage hp for danger warning
    int         hp_attention;   // percentage hp for danger attention
    char        race;           // preselected race
    char        cls;            // preselected class
    bool        terse_hand;     // use terse description for wielded item
    bool        delay_message_clear; // avoid clearing messages each turn
    unsigned    friend_brand;   // Attribute for branding friendly monsters
    bool        no_dark_brand;  // Attribute for branding friendly monsters
    bool        macro_meta_entry; // Allow user to use \{foo} sequences when
                                  // creating macros

    int         fire_items_start; // index of first item for fire command
    FixedVector<int, NUM_FIRE_TYPES>  fire_order; // order for 'f' command

    bool        auto_list;      // automatically jump to appropriate item lists

    bool        flush_input[NUM_FLUSH_REASONS]; // when to flush input buff
    bool        lowercase_invocations;          // prefer lowercase invocations

    char_set_type  char_set;
    FixedVector<unsigned char, NUM_DCHAR_TYPES> char_table;

    int         num_colours;    // used for setting up curses colour table (8 or 16)
    
#ifdef WIZARD
    int         wiz_mode;       // yes, no, never in wiz mode to start
#endif

    // internal use only:
    int         sc_entries;     // # of score entries
    int         sc_format;      // Format for score entries


    std::string map_file_name;	// name of mapping file to use
    std::vector<text_pattern> banned_objects;  // Objects we'll never pick up
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

    bool        item_colour;    // Colour items on level map

    unsigned    detected_monster_colour;    // Colour of detected monsters
    unsigned    detected_item_colour;       // Colour of detected items

    unsigned    heap_brand;     // Highlight heaps of items in the playing area
    unsigned    stab_brand;     // Highlight monsters that are stabbable
    unsigned    may_stab_brand; // Highlight potential stab candidates

    int         explore_stop;      // Stop exploring if a previously unseen
                                   // item comes into view

    std::vector<sound_mapping> sound_mappings;
    std::vector<colour_mapping> menu_colour_mappings;

    int        sort_menus;        // 0 = always, -1 = never, number = beyond
                                  // that size.

    int         dump_kill_places; // How to dump place information for kills.
    int         dump_message_count; // How many old messages to dump

    int         dump_item_origins;  // Show where items came from?
    int         dump_item_origin_price;

    // Order of sections in the character dump.
    std::vector<std::string> dump_order;

    bool        safe_zero_exp; // If true, you feel safe around 0xp monsters
    bool        target_zero_exp;    // If true, targeting targets zero-exp
                                    // monsters.
    bool        target_wrap;        // Wrap around from last to first target
    bool        target_oos;         // 'x' look around can target out-of-LOS
    bool        target_los_first;   // 'x' look around first goes to visible
                                    // objects/features, then goes to stuff
                                    // outside LOS.

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
    int         prev_ck, prev_dk, prev_pr;
    int         prev_weapon;
    int         prev_book;
    bool        prev_randpick;

public:
    // Convenience accessors for the second-class options in named_options.
    int         o_int(const char *name, int def = 0) const;
    long        o_long(const char *name, long def = 0L) const;
    bool        o_bool(const char *name, bool def = false) const;
    std::string o_str(const char *name, const char *def = NULL) const;
    int         o_colour(const char *name, int def = LIGHTGREY) const;

private:
    void set_default_activity_interrupts();
    void clear_activity_interrupts(FixedVector<bool, NUM_AINTERRUPTS> &eints);
    void set_activity_interrupt(
        FixedVector<bool, NUM_AINTERRUPTS> &eints,
        const std::string &interrupt);
    void set_activity_interrupt(const std::string &activity_name,
                                const std::string &interrupt_names,
                                bool append_interrupts);
    void add_dump_fields(const std::string &text);
    void do_kill_map(const std::string &from, const std::string &to);

    static const std::string interrupt_prefix;
};

extern game_options  Options;

struct tagHeader 
{
    short tagID;
    long offset;
};

struct scorefile_entry 
{
public:
    char        version;
    char        release;
    long        points;
    char        name[kNameLen];
    long        uid;                // for multiuser systems
    char        race;
    char        cls;
    char        race_class_name[5]; // overrides race & cls if non-null
    char        lvl;                // player level.
    char        best_skill;         // best skill #
    char        best_skill_lvl;     // best skill level
    int         death_type;
    int         death_source;       // 0 or monster TYPE
    int         mon_num;            // sigh...
    char        death_source_name[40];    // overrides death_source
    char        auxkilldata[ITEMNAME_SIZE]; // weapon wielded, spell cast, etc
    char        dlvl;               // dungeon level (relative)
    char        level_type;         // what kind of level died on..
    char        branch;             // dungeon branch
    int         final_hp;           // actual current HPs (probably <= 0)
    int         final_max_hp;       // net HPs after rot
    int         final_max_max_hp;   // gross HPs before rot
    int         damage;             // damage of final attack
    int         str;                // final str (useful for nickname)
    int         intel;              // final int
    int         dex;                // final dex (useful for nickname)
    int         god;                // god
    int         piety;              // piety 
    int         penance;            // penance
    char        wiz_mode;           // character used wiz mode
    time_t      birth_time;         // start time of character
    time_t      death_time;         // end time of character
    long        real_time;          // real playing time in seconds
    long        num_turns;          // number of turns taken 
    int         num_diff_runes;     // number of rune types in inventory
    int         num_runes;          // total number of runes in inventory

public:
    scorefile_entry();
    scorefile_entry(int damage, int death_source, int death_type,
                    const char *aux, bool death_cause_only = false);

    void init_death_cause(int damage, int death_source, int death_type,
                          const char *aux);
    void init();
    void reset();

    enum death_desc_verbosity {
        DDV_TERSE,
        DDV_ONELINE,
        DDV_NORMAL,
        DDV_VERBOSE
    };

    std::string hiscore_line(death_desc_verbosity verbosity) const;

    std::string character_description(death_desc_verbosity) const;
    // Full description of death: Killed by an xyz wielding foo
    std::string death_description(death_desc_verbosity) const;

    std::string death_place(death_desc_verbosity) const;

    std::string game_time(death_desc_verbosity) const;

private:
    std::string single_cdesc() const;
    std::string strip_article_a(const std::string &s) const;
    std::string terse_missile_cause() const;
    std::string terse_beam_cause() const;
    std::string terse_wild_magic() const;
    std::string terse_trap() const;
    const char *damage_verb() const;
    const char *death_source_desc() const;
    std::string damage_string(bool terse = false) const;
};

extern const struct coord_def Compass[8];

#endif // EXTERNS_H
