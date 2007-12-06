/*
 *  File:       mon-util.h
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MONUTIL_H
#define MONUTIL_H

#include "externs.h"
#include "enum.h"

enum corpse_effect_type
{
    CE_NOCORPSE,                       //    0
    CE_CLEAN,                          //    1
    CE_CONTAMINATED,                   //    2
    CE_POISONOUS,                      //    3
    CE_HCL,                            //    4
    CE_MUTAGEN_RANDOM,                 //    5
    CE_MUTAGEN_GOOD, //    6 - may be worth implementing {dlb}
    CE_MUTAGEN_BAD, //    7 - may be worth implementing {dlb}
    CE_RANDOM, //    8 - not used, but may be worth implementing {dlb}
    CE_ROTTEN = 50 //   50 - must remain at 50 for now {dlb}
};

enum gender_type
{
    GENDER_NEUTER,
    GENDER_MALE,
    GENDER_FEMALE
};

enum mon_attack_type
{
    AT_NONE,
    AT_HIT,         // including weapon attacks
    AT_BITE,
    AT_STING,
    AT_SPORE,
    AT_TOUCH,
    AT_ENGULF,
    AT_CLAW,
    AT_TAIL_SLAP,
    AT_BUTT,
    
    AT_SHOOT        // attack representing missile damage for M_ARCHER
};

enum mon_attack_flavour
{
    AF_PLAIN,
    AF_ACID,
    AF_BLINK,
    AF_COLD,
    AF_CONFUSE,
    AF_DISEASE,
    AF_DRAIN_DEX,
    AF_DRAIN_STR,
    AF_DRAIN_XP,
    AF_ELEC,
    AF_FIRE,
    AF_HUNGER,
    AF_MUTATE,
    AF_BAD_MUTATE,
    AF_PARALYSE,
    AF_POISON,
    AF_POISON_NASTY,
    AF_POISON_MEDIUM,
    AF_POISON_STRONG,
    AF_POISON_STR,
    AF_ROT,
    AF_VAMPIRIC,
    AF_KLOWN,
    AF_DISTORT,
    AF_RAGE
};

// properties of the monster class (other than resists/vulnerabilities)
enum mons_class_flags
{
    M_NO_FLAGS          = 0,

    M_SPELLCASTER       = (1<< 0),        // any non-physical-attack powers,
    M_ACTUAL_SPELLS     = (1<< 1),        // monster is a wizard,
    M_PRIEST            = (1<< 2),        // monster is a priest
    M_FIGHTER           = (1<< 3),        // monster is skilled fighter

    M_FLIES             = (1<< 4),        // will crash to ground if paralysed?
    M_LEVITATE          = (1<< 5),        // ... but not if this is set
    M_INVIS             = (1<< 6),        // is created invis
    M_SEE_INVIS         = (1<< 7),        // can see invis
    M_SENSE_INVIS       = (1<< 8),        // can sense invisible things
    M_SPEAKS            = (1<< 9),        // uses talking code
    M_CONFUSED          = (1<<10),        // monster is perma-confused,
    M_BATTY             = (1<<11),        // monster is batty
    M_SPLITS            = (1<<12),        // monster can split
    M_AMPHIBIOUS        = (1<<13),        // monster can swim in water,
    M_THICK_SKIN        = (1<<14),        // monster has more effective AC,
    M_HUMANOID          = (1<<15),        // for Glamour 
    M_COLD_BLOOD        = (1<<16),        // susceptible to cold
    M_WARM_BLOOD        = (1<<17),        // no effect currently
    M_REGEN             = (1<<18),        // regenerates quickly
    M_BURROWS           = (1<<19),        // monster digs through rock
    M_EVIL              = (1<<20),        // monster vulnerable to holy spells

    M_UNIQUE            = (1<<21),        // monster is a unique
    M_ACID_SPLASH       = (1<<22),        // Passive acid splash when hit.

    M_ARCHER            = (1<<23),        // gets various archery boosts

    M_WALL_SHIELDED     = (1<<24),        // Shielded from attacks if in wall

    M_TWOWEAPON         = (1<<25),        // wields two weapons at once

    M_SPECIAL_ABILITY   = (1<<26),        // XXX: eventually make these spells?

    M_NO_SKELETON       = (1<<29),        // boneless corpses
    M_NO_EXP_GAIN       = (1<<31)         // worth 0 xp
};

enum mon_event_type
{
    ME_EVAL,                            // 0, evaluate monster AI state
    ME_DISTURB,                         // noisy
    ME_ANNOY,                           // annoy at range
    ME_ALERT,                           // alert to presence
    ME_WHACK,                           // physical attack
    ME_SHOT,                            // attack at range
    ME_SCARE,                           // frighten monster
    ME_CORNERED                         // cannot flee
};

enum mon_intel_type             // Must be in increasing intelligence order
{
    I_PLANT = 0,
    I_INSECT,
    I_ANIMAL,
    I_NORMAL,
    I_HIGH
};

// order of these is important:
enum mon_itemuse_type
{
    MONUSE_NOTHING,
    MONUSE_EATS_ITEMS,
    MONUSE_OPEN_DOORS,
    MONUSE_STARTING_EQUIPMENT,
    MONUSE_WEAPONS_ARMOUR, 
    MONUSE_MAGIC_ITEMS
};

// now saved in an unsigned long.
enum mon_resist_flags
{
    MR_NO_FLAGS          = 0,

    // resistances 
    // Notes: 
    // - negative energy is mostly handled via mons_has_life_force()
    // - acid is handled mostly by genus (jellies) plus non-living
    // - asphyx-resistance replaces hellfrost resistance.
    MR_RES_ELEC          = (1<< 0),
    MR_RES_POISON        = (1<< 1),
    MR_RES_FIRE          = (1<< 2),
    MR_RES_HELLFIRE      = (1<< 3),
    MR_RES_COLD          = (1<< 4),
    MR_RES_ASPHYX        = (1<< 5),
    MR_RES_ACID          = (1<< 6),

    // vulnerabilities
    MR_VUL_ELEC          = (1<< 7),
    MR_VUL_POISON        = (1<< 8),
    MR_VUL_FIRE          = (1<< 9),
    MR_VUL_COLD          = (1<<10),

    // melee armour resists/vulnerabilities 
    // XXX: how to do combos (bludgeon/slice, bludgeon/pierce)
    MR_RES_PIERCE        = (1<<11),
    MR_RES_SLICE         = (1<<12),
    MR_RES_BLUDGEON      = (1<<13),

    MR_VUL_PIERCE        = (1<<14),
    MR_VUL_SLICE         = (1<<15),
    MR_VUL_BLUDGEON      = (1<<16)
};

enum shout_type
{
    S_SILENT,               // silent
    S_SHOUT,                // shout                                           
    S_BARK,                 // bark
    S_SHOUT2,               // shout twice (e.g. two-headed ogres)
    S_ROAR,                 // roar
    S_SCREAM,               // scream
    S_BELLOW,               // bellow (?)
    S_SCREECH,              // screech
    S_BUZZ,                 // buzz
    S_MOAN,                 // moan
    S_WHINE,                // irritating whine (mosquito)
    S_CROAK,                // frog croak
    S_GROWL,                // for bears
    S_HISS,                 // for snakes and lizards

    // Loudness setting for shouts that are only defined in dat/shout.txt
    S_VERY_SOFT,
    S_SOFT,
    S_NORMAL,
    S_LOUD,
    S_VERY_LOUD,

    NUM_SHOUTS,
    S_RANDOM
};

enum zombie_size_type
{
    Z_NOZOMBIE,
    Z_SMALL,
    Z_BIG
};

struct bolt;

// ($pellbinder) (c) D.G.S.E. 1998

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

// Amount of monster->speed_increment used by different actions; defaults
// to 10.
struct mon_energy_usage
{
    char move;
    char swim;
    char attack;
    char missile; // Arrows/crossbows/etc
    char spell;
    char special;
    char item;    // Using an item (i.e., drinking a potion)

    // Percent of monster->speed used when picking up an item; defaults
    // to 100%
    char pickup_percent; 
};

struct monsterentry
{
    short mc;            // monster number

    unsigned char showchar, colour;
    const char *name;

    unsigned long bitfields;
    unsigned long resists;

    short weight;
    // experience is calculated like this:
    // ((((max_hp / 7) + 1) * (mHD * mHD) + 1) * exp_mod) / 10
    //     ^^^^^^ see below at hpdice
    //   Note that this may make draining attacks less attractive (LRH)
    char exp_mod;

    monster_type genus,         // "team" the monster plays for
                 species;       // corpse type of the monster

    mon_holy_type holiness;

    short resist_magic;  // (positive is ??)
    // max damage in a turn is total of these four?

    mon_attack_def attack[4];

    // hpdice[4]: [0]=HD [1]=min_hp [2]=rand_hp [3]=add_hp
    // min hp = [0]*[1]+[3] & max hp = [0]*([1]+[2])+[3])
    // example: the Iron Golem, hpdice={15,7,4,0}
    //      15*7 < hp < 15*(7+4),
    //       105 < hp < 165
    // hp will be around 135 each time.
    unsigned       hpdice[4];

    char AC;  // armour class
    char ev; // evasion
    mon_spellbook_type sec;
    corpse_effect_type corpse_thingy;
    zombie_size_type zombie_size;
    shout_type shouts;
    mon_intel_type intel;

    char             speed;        // How quickly speed_increment increases
    mon_energy_usage energy_usage; // And how quickly it decreases
    mon_itemuse_type gmon_use;
    size_type size;
};

monsterentry *get_monster_data(int p_monsterid);

// last updated 10jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void init_monsters( FixedVector<unsigned short, 1000>& colour );
void init_monster_symbols();

monsters *monster_at(const coord_def &pos);

// this is the old moname()
std::string mons_type_name(int type, description_level_type desc );
                  
// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - direct - fight - monstuff - mstuff2 - spells4 - view
 * *********************************************************************** */
flight_type mons_class_flies( int mc );
flight_type mons_flies( const monsters *mon );


// last updated XXmay2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - monstuff
 * *********************************************************************** */
mon_itemuse_type mons_itemuse(int mc);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - monstuff - view
 * *********************************************************************** */
bool mons_see_invis(const monsters *mon);
bool mons_sense_invis(const monsters *mon);
bool mons_monster_visible( const monsters *mon, const monsters *targ );
bool mons_player_visible( const monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: view
 * *********************************************************************** */
int get_shout_noise_level(const shout_type shout);
shout_type mons_shouts(int mclass);

bool mons_is_unique(int mclass);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: describe - fight
 * *********************************************************************** */
// int exper_value(int mclass, int mHD, int maxhp);
int exper_value( const struct monsters *monster );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - mon-util
 * *********************************************************************** */
int hit_points(int hit_dice, int min_hp, int rand_hp);

int mons_type_hit_dice( int type );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - effects
 * *********************************************************************** */
int mons_resist_magic( const monsters *mon );
int mons_resist_turn_undead( const monsters *mon );
bool mons_immune_magic( const monsters *mon );
const char* mons_resist_string(const monsters *mon);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: fight - monstuff
 * *********************************************************************** */
int mons_damage(int mc, int rt);
mon_attack_def mons_attack_spec(const monsters *, int rt);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: food - spells4
 * *********************************************************************** */
corpse_effect_type mons_corpse_effect(int mc);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight - monstuff - mon-util
 * *********************************************************************** */
bool mons_class_flag(int mc, int bf);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - effects - fight - monstuff - mstuff2 - spells2 -
 *              spells3 - spells4
 * *********************************************************************** */
mon_holy_type mons_class_holiness(int mclass);
mon_holy_type mons_holiness(const monsters *);

bool mons_is_mimic( int mc ); 
bool mons_is_statue(int mc);
bool mons_is_demon( int mc );
bool mons_is_humanoid( int mc );

bool mons_wields_two_weapons(const monsters *m);
bool mons_wields_two_weapons(monster_type m);
bool mons_is_summoned(const monsters *m);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: monstuff - spells4 - view
 * *********************************************************************** */
mon_intel_type mons_intel(int mclass);

bool intelligent_ally(const monsters *mon);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - monstuff
 * *********************************************************************** */
int mons_res_cold( const monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - spells4
 * *********************************************************************** */
int mons_res_elec( const monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - monstuff
 * *********************************************************************** */
int mons_res_fire( const monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - monstuff - spells4
 * *********************************************************************** */
int mons_res_poison( const monsters *mon );
int mons_res_acid( const monsters *mon );
int mons_res_negative_energy( const monsters *mon );

bool mons_res_asphyx( const monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - items - spells2 - spells4
 * *********************************************************************** */
int mons_skeleton(int mcls);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: describe - fight - items - misc - monstuff - mon-util -
 *              player - spells2 - spells3
 * *********************************************************************** */
int mons_weight(int mclass);


// last updated 08may2001 {gdl}
/* ***********************************************************************
 * called from: monplace mon-util
 * *********************************************************************** */
int mons_speed(int mclass);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - mon-util - spells2
 * *********************************************************************** */
int mons_zombie_size(int mclass);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: bang - beam - monstuff - mstuff2 - spells4 - view
 * *********************************************************************** */
// unsigned char mons_category(int which_mons);


// last updated 07jan2001 (gdl)
/* ***********************************************************************
 * called from: beam
 * *********************************************************************** */
int mons_power(int mclass);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: spells2 - view
 * *********************************************************************** */
unsigned mons_char(int mc);


// last updated 10jun2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight - misc
 * *********************************************************************** */
int mons_class_colour(int mc);
int mons_colour(const monsters *m);

void mons_load_spells( monsters *mon, mon_spellbook_type book );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight
 * *********************************************************************** */
void define_monster(int mid);
void define_monster(monsters &mons);

// last updated 4jan2001 (gdl)
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
bool mons_should_fire(struct bolt &beam);


// last updated 23mar2001 (gdl)
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
bool ms_direct_nasty(spell_type monspell);

// last updated 14jan2001 (gdl)
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
bool ms_requires_tracer(spell_type mons_spell);

bool ms_useful_fleeing_out_of_sight( const monsters *mon, spell_type monspell );
bool ms_quick_get_away( const monsters *mon, spell_type monspell );
bool ms_waste_of_time( const monsters *mon, spell_type monspell );
bool ms_low_hitpoint_cast( const monsters *mon, spell_type monspell );

bool mons_has_ranged_spell( const monsters *mon );
bool mons_has_ranged_attack( const monsters *mon );
bool mons_is_magic_user( const monsters *mon );

// last updated 06mar2001 (gdl)
/* ***********************************************************************
 * called from:
 * *********************************************************************** */
const char *mons_pronoun(monster_type mon_type, pronoun_type variant);

// last updated 14mar2001 (gdl)
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
bool mons_aligned(int m1, int m2);

// last updated 14mar2001 (gdl)
/* ***********************************************************************
 * called from: monstuff acr
 * *********************************************************************** */
bool mons_friendly(const monsters *m);
bool mons_neutral(const monsters *m);
mon_attitude_type mons_attitude(const monsters *m);

bool mons_behaviour_perceptible(const monsters *mons);
bool mons_is_confused(const monsters *m);
bool mons_is_caught(const monsters *m);
bool mons_is_fleeing(const monsters *m);
bool mons_is_sleeping(const monsters *m);
bool mons_is_batty(const monsters *m);
bool mons_was_seen(const monsters *m);
bool mons_is_known_mimic(const monsters *m);
bool mons_is_evil( const monsters *mon );
bool mons_is_unholy( const monsters *mon );
bool mons_is_icy(const monsters *mons);
bool mons_is_icy(int mtype);
bool mons_has_lifeforce( const monsters *mon );
monster_type mons_genus( int mc );
monster_type mons_species( int mc );

bool mons_looks_stabbable(const monsters *m);

bool mons_looks_distracted(const monsters *m);

bool check_mons_resist_magic( const monsters *monster, int pow );

bool mons_class_is_stationary(int monsclass);
bool mons_class_is_confusable(int monsclass);
bool mons_class_is_slowable(int monsclass);
bool mons_is_stationary(const monsters *mons);
bool mons_is_insubstantial(int type);
bool mons_is_submerged( const monsters *mon );

bool invalid_monster(const monsters *mons);
bool invalid_monster_class(int mclass);

bool monster_shover(const monsters *m);
bool mons_is_paralysed(const monsters *m);

bool monster_senior(const monsters *first, const monsters *second);
monster_type draco_subspecies( const monsters *mon );

monster_type random_monster_at_grid(int x, int y);
monster_type random_monster_at_grid(int grid);

void         init_mon_name_cache();
monster_type get_monster_by_name(std::string name, bool exact = false);

std::string do_mon_str_replacements(const std::string &msg,
                                    const monsters* monster);

enum mon_body_shape {
    MON_SHAPE_HUMANOID,
    MON_SHAPE_HUMANOID_WINGED,
    MON_SHAPE_HUMANOID_TAILED,
    MON_SHAPE_HUMANOID_WINGED_TAILED,
    MON_SHAPE_CENTAUR,
    MON_SHAPE_NAGA,
    MON_SHAPE_QUADRUPED,
    MON_SHAPE_QUADRUPED_TAILLESS,
    MON_SHAPE_QUADRUPED_WINGED,
    MON_SHAPE_BAT,
    MON_SHAPE_SNAKE, // Including eels and worms
    MON_SHAPE_FISH,
    MON_SHAPE_INSECT,
    MON_SHAPE_INSECT_WINGED,
    MON_SHAPE_ARACHNID,
    MON_SHAPE_CENTIPEDE,
    MON_SHAPE_SNAIL,
    MON_SHAPE_PLANT,
    MON_SHAPE_FUNGUS,
    MON_SHAPE_ORB,
    MON_SHAPE_BLOB,
    MON_SHAPE_MISC
};

mon_body_shape get_mon_shape(const monsters *mon);
mon_body_shape get_mon_shape(const int type);

std::string get_mon_shape_str(const monsters *mon);
std::string get_mon_shape_str(const int type);
std::string get_mon_shape_str(const mon_body_shape shape);

bool mons_class_can_pass(const int mclass, const dungeon_feature_type grid);

#endif
