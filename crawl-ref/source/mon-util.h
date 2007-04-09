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

// ($pellbinder) (c) D.G.S.E. 1998

// zombie size
#define Z_NOZOMBIE 0            // no zombie (and no skeleton)
#define Z_SMALL 1
#define Z_BIG 2
// Note - this should be set to 0 for classed monsters, eg orc wizard

// shout
#define S_RANDOM -1
#define S_SILENT 0
#define S_SHOUT 1               //1=shout
#define S_BARK 2                //2=bark
#define S_SHOUT2 3              //3=shout twice
#define S_ROAR 4                //4=roar
#define S_SCREAM 5              //5=scream
#define S_BELLOW 6              //6=bellow (?)
#define S_SCREECH 7             //7=screech
#define S_BUZZ 8                //8=buzz
#define S_MOAN 9                //9=moan
#define S_WHINE 10              //10=irritating whine (mosquito)
#define S_CROAK 11              //11=frog croak
#define S_GROWL 12              //jmf: for bears
#define S_HISS 13               //bwr: for snakes and lizards

// ai
// So far this only affects a) chance to see stealthy player and b) chance to
//  walk through damaging clouds (LRH)
//jmf: now has relevence to mind-affecting spells, maybe soon behaviour
#define I_PLANT 0
#define I_INSECT 1
#define I_ANIMAL 2
#define I_NORMAL 3
#define I_HIGH 4
#define I_ANIMAL_LIKE 5
#define I_REPTILE 6

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

    mon_holy_type holiness;       // -1=holy,0=normal,1=undead,2=very very evil

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

    char AC;             // armour class

    char ev;             // evasion

    char speed, speed_inc;        // duh!

    short sec;           // not used (250) most o/t time

    // eating the corpse: 1=clean,2=might be contaminated,3=poison,4=very bad
    corpse_effect_type corpse_thingy;
    // 0=no zombie, 1=small zombie (z) 107, 2=_BIG_ zombie (Z) 108
    char zombie_size;
  // 0-12: see above, -1=random one of (0-7)
    char shouts;
  // AI things?
    char intel;          // 0=none, 1=worst...4=best

    char gmon_use;

    size_type size;
};    // mondata[] - again, no idea why this was externed {dlb}


// wow. this struct is only about 48 bytes, (excluding the name)


// last updated 10jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void init_monsters( FixedVector<unsigned short, 1000>& colour );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: bang - beam - debug - direct - effects - fight - item_use -
 *              monstuff - mstuff2 - ouch - spells1 - spells2 - spells3 -
 *              spells4
 * *********************************************************************** */
// mons_wpn only important for dancing weapons -- bwr
const char *monam(const monsters *mon,
                  int mons_num, int mons, bool vis, char desc,
                  int mons_wpn = NON_ITEM);

// these front for monam
const char *ptr_monam(const monsters *mon, char desc, bool force_seen = false);
std::string str_monam(const monsters *mon, description_level_type desc,
                      bool force_seen = false);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - direct - fight - monstuff - mstuff2 - spells4 - view
 * *********************************************************************** */
int mons_class_flies( int mc );
int mons_flies( const monsters *mon );


// last updated XXmay2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - monstuff
 * *********************************************************************** */
char mons_itemuse(int mc);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - monstuff - view
 * *********************************************************************** */
bool mons_see_invis(const monsters *mon);
bool mons_sense_invis(const monsters *mon);
bool mons_monster_visible( struct monsters *mon, struct monsters *targ );
bool mons_player_visible( struct monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: view
 * *********************************************************************** */
int mons_shouts(int mclass);

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
bool mons_is_summoned(const monsters *m);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: monstuff - spells4 - view
 * *********************************************************************** */
int mons_intel(int mclass);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: spells4
 * *********************************************************************** */
int mons_intel_type(int mclass); //jmf: added 20mar2000


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
unsigned char mons_char(int mc);


// last updated 10jun2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight - misc
 * *********************************************************************** */
int mons_class_colour(int mc);
int mons_colour(const monsters *m);

void mons_load_spells( monsters *mon, int book );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight
 * *********************************************************************** */
void define_monster(int mid);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: debug - itemname - mon-util
 * *********************************************************************** */
const char *moname(int mcl, bool vis, char descrip, char glog[ ITEMNAME_SIZE ]);


#ifdef DEBUG_DIAGNOSTICS
// last updated 25sep2001 {dlb}
/* ***********************************************************************
 * called from: describe
 * *********************************************************************** */
extern const char *mons_spell_name(spell_type);
#endif

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
bool ms_waste_of_time( const monsters *mon, spell_type monspell );
bool ms_low_hitpoint_cast( const monsters *mon, spell_type monspell );

bool mons_has_ranged_spell( const monsters *mon );
bool mons_has_ranged_attack( const monsters *mon );
bool mons_is_magic_user( const monsters *mon );

// last updated 06mar2001 (gdl)
/* ***********************************************************************
 * called from:
 * *********************************************************************** */
const char *mons_pronoun(int mon_type, int variant);

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

bool mons_is_confused(const monsters *m);
bool mons_is_fleeing(const monsters *m);
bool mons_is_sleeping(const monsters *m);
bool mons_is_batty(const monsters *m);
bool mons_was_seen(const monsters *m);
bool mons_is_known_mimic(const monsters *m);
bool mons_is_evil( const monsters *mon );
bool mons_is_unholy( const monsters *mon );
bool mons_has_lifeforce( const monsters *mon );
monster_type mons_genus( int mc );
monster_type mons_species( int mc );

bool mons_looks_stabbable(const monsters *m);

bool mons_looks_distracted(const monsters *m);

bool check_mons_resist_magic( const monsters *monster, int pow );

bool mons_class_is_stationary(int monsclass);
bool mons_is_stationary(const monsters *mons);
bool mons_is_submerged( const monsters *mon );

bool invalid_monster(const monsters *mons);
bool invalid_monster_class(int mclass);

bool monster_shover(const monsters *m);
bool mons_is_paralysed(const monsters *m);

bool monster_senior(const monsters *first, const monsters *second);
monster_type draco_subspecies( const monsters *mon );

monster_type random_monster_at_grid(int x, int y);
monster_type random_monster_at_grid(int grid);

monster_type get_monster_by_name(std::string name, bool exact = false);

#endif
