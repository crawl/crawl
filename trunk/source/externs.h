/*
 *  File:       externs.h
 *  Summary:    Fixed size 2D vector class that asserts if you do something bad.
 *  Written by: Linley Henzell
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

#include <queue>

#include <time.h>

#include "defines.h"
#include "enum.h"
#include "FixAry.h"
#include "message.h"

#define INFO_SIZE       200          // size of message buffers
#define ITEMNAME_SIZE   200          // size of item names/shop names/etc
#define HIGHSCORE_SIZE  800          // <= 10 Lines for long format scores

#define MAX_NUM_GODS    21

extern char info[INFO_SIZE];         // defined in acr.cc {dlb}

extern unsigned char show_green;     // defined in view.cc {dlb}

// defined in mon-util.cc -- w/o this screen redraws *really* slow {dlb}
extern FixedVector<unsigned short, 1000> mcolour;  

#ifdef SHORT_FILE_NAMES
    const int kNameLen = 30;
    const int kFileNameLen = 6;
    const int kFileNameSize = 5 + kFileNameLen;

#else
    #ifdef SAVE_DIR_PATH
        // file name length has to be able to cover full paths -- bwross
        const int kNameLen = 30;
        const int kFileNameLen = 250;
        const int kFileNameSize = 5 + kFileNameLen;
    #else
        const int kNameLen = 30;
        const int kFileNameLen = 28;
        const int kFileNameSize = 5 + kFileNameLen;
    #endif
#endif


// Length of Path + File Name
const int kPathLen = 256;

// This value is used to mark that the current berserk is free from
// penalty (Xom's granted or from a deck of cards).
#define NO_BERSERK_PENALTY    -1

struct coord_def
{
    int         x;
    int         y;

    // coord_def( int x_in = 0, int y_in = 0 ) : x(x_in), y(y_in) {};
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
    char        beam_name[40];
    bool        isBeam;                // beams? (can hits multiple targets?)
    const char *aux_source;            // source of KILL_MISC beams

    // OUTPUT parameters (tracing, ID)
    bool        obviousEffect;         // did an 'obvious' effect happen?
    int         fr_count, foe_count;   // # of times a friend/foe is "hit"
    int         fr_power, foe_power;   // total levels/hit dice affected

    // INTERNAL use - should not usually be set outside of beam.cc
    bool        isTracer;      // is this a tracer?
    bool        aimedAtFeet;   // this was aimed at self!
    bool        msgGenerated;  // an appropriate msg was already mpr'd
    bool        isExplosion;   // explosion phase (as opposed to beam phase)
    bool        smartMonster;  // tracer firer can guess at other mons. resists?
    bool        canSeeInvis;   // tracer firer can see invisible?
    bool        isFriendly;    // tracer firer is enslaved or pet
    int         foeRatio;      // 100* foe ratio (see mons_should_fire())
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
    unsigned long  flags;      // item statuc flags
    short          quantity;   // number of items

    short  x;          // x-location;         for inventory items = -1 
    short  y;          // y-location;         for inventory items = -1
    short  link;       // link to next item;  for inventory items = slot
};


struct player
{
  char turn_is_over; // flag signaling that player has performed a timed action

  unsigned char prev_targ;
  char your_name[kNameLen];

  unsigned char species;

  char run_x;
  char run_y;
  FixedVector< run_check_dir, 3 > run_check; // array of grids to check
  char running;

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
  char redraw_hit_points;
  char redraw_magic_points;
  char redraw_strength;
  char redraw_intelligence;
  char redraw_dexterity;
  char redraw_experience;
  char redraw_armour_class;

  char redraw_gold;
  char redraw_evasion;

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

  std::queue< delay_queue_item >  delay_queue;  // pending actions

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
  char level_type;

  char where_are_you;

  FixedVector<unsigned char, 30> branch_stairs;

  char religion;
  unsigned char piety;
  unsigned char gift_timeout;
  FixedVector<unsigned char, MAX_NUM_GODS>  penance;
  FixedVector<unsigned char, MAX_NUM_GODS>  worshipped;


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
};

extern struct player you;

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
    unsigned char attitude;            // from MONS_ATTITUDE
    unsigned int behaviour;
    unsigned int foe;
    FixedVector<unsigned int, NUM_MON_ENCHANTS> enchantment;
    unsigned char flags;               // bitfield of boolean flags
    unsigned int number;               // #heads (hydra), etc.
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

    FixedArray< unsigned char, GXM, GYM >    map;   // discovered terrain

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


extern void (*viewwindow) (char, bool);


struct system_environment
{
    char *crawl_name;
    char *crawl_pizza;
    char *crawl_rc;
    char *crawl_dir;
    char *home;                 // only used by MULTIUSER systems
    bool  board_with_nail;      // Easter Egg silliness
};

extern system_environment SysEnv;

struct game_options 
{
    long        autopickups;    // items to autopickup
    bool        verbose_dump;   // make character dumps contain more detail
    bool        colour_map;     // add colour to the map
    bool        clean_map;      // remove unseen clouds/monsters
    bool        show_uncursed;  // label known uncursed items as "uncursed"
    bool        always_greet;   // display greeting message when reloading
    bool        easy_open;      // open doors with movement
    bool        easy_armour;    // allow auto-removing of armour
    bool        easy_butcher;   // open doors with movement
    int         easy_confirm;   // make yesno() confirming easier
    int         easy_quit_item_prompts; // make item prompts quitable on space
    int         colour[16];     // macro fg colours to other colours
    int         background;     // select default background colour
    int         channels[NUM_MESSAGE_CHANNELS];  // msg channel colouring
    int         weapon;         // auto-choose weapon for character
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
    unsigned int friend_brand;  // Attribute for branding friendly monsters
    bool        no_dark_brand;  // Attribute for branding friendly monsters

    int         fire_items_start; // index of first item for fire command
    FixedVector<int, NUM_FIRE_TYPES>  fire_order; // order for 'f' command

    bool        auto_list;      // automatically jump to appropriate item lists

    bool        flush_input[NUM_FLUSH_REASONS]; // when to flush input buff
    bool        lowercase_invocations;          // prefer lowercase invocations

#ifdef CURSES
    int         num_colours;    // used for setting up colour table (8 or 16)
#endif
    
#ifdef WIZARD
    int         wiz_mode;       // yes, no, never in wiz mode to start
#endif

    // internal use only:
    int         sc_entries;     // # of score entries
    int         sc_format;      // Format for score entries
};

extern game_options  Options;

struct tagHeader 
{
    short tagID;
    long offset;
};

struct scorefile_entry 
{
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
};

#endif // EXTERNS_H
