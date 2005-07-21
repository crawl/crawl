/*
 *  File:       mon-util.h
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MONUTIL_H
#define MONUTIL_H

#include "externs.h"

// ($pellbinder) (c) D.G.S.E. 1998

// ****remember***** must make an hardcopy of this sometime

#if defined(macintosh) || defined(__IBMCPP__) || defined(SOLARIS) || defined(__BCPLUSPLUS__) || defined(BSD)
#define PACKED
#else
#define PACKED __attribute__ ((packed))
#endif

// leaves no skeleton? ("blob" monsters?)
// if weight=0 or zombie_size=0, this is always true
#define M_NO_FLAGS 0            // clear
#define M_NO_SKELETON (1<<0)
// resistances
#define M_RES_ELEC (1<<1)
#define M_RES_POISON (1<<2)
#define M_RES_FIRE (1<<3)
#define M_RES_HELLFIRE (1<<4)
#define M_RES_COLD (1<<5)
// invisible
#define M_INVIS (1<<6)          // is created with invis enchantment set, and never runs out
// vulnerabilities
//#define M_ED_ELEC (1<<6) // never used
#define M_ED_POISON (1<<7)      // ???  - - This flag is now (2.50) set for insects (LRH)
#define M_ED_FIRE (1<<8)
#define M_ED_COLD (1<<9)
#define M_SPELLCASTER (1<<10)    // any non-physical-attack powers
#define M_FLIES (1<<11)          // will crash to ground if paralysed (wings)
#define M_LEVITATE (1<<12)       // not if this is set
#define M_SEE_INVIS (1<<13)
// killing this beast only gives 10 experience (makes sense for plants/fungi)
#define M_NO_EXP_GAIN (1<<14)    // must do this manually
#define M_SPEAKS (1<<15)
//jmf: M_SPELLCASTER was taken ... :-b
#define M_ACTUAL_SPELLS (1<<16)  // monster is a wizard
#define M_PRIEST (1<<17)         // monster is a priest of Brian's Orc God (BOG)
#define M_COLD_BLOOD (1<<18)
#define M_WARM_BLOOD (1<<19)
#define M_CONFUSED (1<<20)       // monster is perma-confused
#define M_SPLITS (1<<21)         // monster is perma-confused
#define M_AMPHIBIOUS (1<<22)     // monster can swim in water

//jmf: it'd be nice if these next two were implimented ...
#define M_ON_FIRE (1<<29)        // flag for Hellion-like colour shift
#define M_FROZEN (1<<30)         // flag for ice-like colour shift


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
    short mc PACKED;            // monster number

    unsigned char showchar PACKED, colour PACKED;
    const char *name /*[32]*/PACKED; //longest is 23 till now (31 is max alowed here)

    int bitfields PACKED;
    short weight PACKED;
    // experience is calculated like this:
    // ((((max_hp / 7) + 1) * (mHD * mHD) + 1) * exp_mod) / 10
    //     ^^^^^^ see below at hpdice
    //   Note that this may make draining attacks less attractive (LRH)
    char exp_mod PACKED;

    unsigned short charclass PACKED;     //

    char holiness PACKED;       // -1=holy,0=normal,1=undead,2=very very evil

    short resist_magic PACKED;  // (positive is ??)
    // max damage in a turn is total of these four?

    unsigned char damage[4] PACKED;

// hpdice[4]: [0]=HD [1]=min_hp [2]=rand_hp [3]=add_hp
// min hp = [0]*[1]+[3] & max hp = [0]*([1]+[2])+[3])
// example: the Iron Golem, hpdice={15,7,4,0}
//      15*7 < hp < 15*(7+4),
//       105 < hp < 165
// hp will be around 135 each time. (assuming an good random number generator)
// !!!!!!! The system is exactly the same as before, only the way of writing changed !!!!
    unsigned char hpdice[4] PACKED;     // until we have monsters with 32767 hp,this is easily possible

    char AC PACKED;             // armour class

    char ev PACKED;             // evasion

    char speed PACKED, speed_inc PACKED;        // duh!

    short sec PACKED;           // not used (250) most o/t time

    // eating the corpse: 1=clean,2=might be contaminated,3=poison,4=very bad
    char corpse_thingy PACKED;
    // 0=no zombie, 1=small zombie (z) 107, 2=_BIG_ zombie (Z) 108
    char zombie_size PACKED;
  // 0-12: see above, -1=random one of (0-7)
    char shouts PACKED;
  // AI things?
    char intel PACKED;          // 0=none, 1=worst...4=best

    char gmon_use PACKED;
};    // mondata[] - again, no idea why this was externed {dlb}


// wow. this struct is only about 48 bytes, (excluding the name)


// last updated 10jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void mons_init( FixedVector<unsigned short, 1000>& colour );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: bang - beam - debug - direct - effects - fight - item_use -
 *              monstuff - mstuff2 - ouch - spells1 - spells2 - spells3 -
 *              spells4
 * *********************************************************************** */
// mons_wpn only important for dancing weapons -- bwr
const char *monam(int mons_num, int mons, bool vis, char desc, int mons_wpn = NON_ITEM);

// these front for monam
const char *ptr_monam( struct monsters *mon, char desc );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - direct - fight - monstuff - mstuff2 - spells4 - view
 * *********************************************************************** */
char mons_class_flies( int mc );
char mons_flies( struct monsters *mon );


// last updated XXmay2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - monstuff
 * *********************************************************************** */
char mons_itemuse(int mc);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - monstuff - view
 * *********************************************************************** */
char mons_see_invis( struct monsters *mon );

bool mons_monster_visible( struct monsters *mon, struct monsters *targ );
bool mons_player_visible( struct monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: view
 * *********************************************************************** */
char mons_shouts(int mclass);

bool mons_is_unique(int mclass);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: describe - fight
 * *********************************************************************** */
// int exper_value(int mclass, int mHD, int maxhp);
int exper_value( struct monsters *monster );


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
int mons_resist_magic( struct monsters *mon );
int mons_resist_turn_undead( struct monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: fight - monstuff
 * *********************************************************************** */
int mons_damage(int mc, int rt);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight - monstuff - spells4
 * *********************************************************************** */
int mons_charclass(int mcls);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: food - spells4
 * *********************************************************************** */
int mons_corpse_thingy(int mclass);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight - monstuff - mon-util
 * *********************************************************************** */
int mons_flag(int mc, int bf);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - effects - fight - monstuff - mstuff2 - spells2 -
 *              spells3 - spells4
 * *********************************************************************** */
int mons_holiness(int mclass);

bool mons_is_mimic( int mc ); 
bool mons_is_demon( int mc ); 
bool mons_is_humanoid( int mc );


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
int mons_res_cold( struct monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - spells4
 * *********************************************************************** */
int mons_res_elec( struct monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - monstuff
 * *********************************************************************** */
int mons_res_fire( struct monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - monstuff - spells4
 * *********************************************************************** */
int mons_res_poison( struct monsters *mon );

int mons_res_negative_energy( struct monsters *mon );


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
unsigned char mons_colour(int mc);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight
 * *********************************************************************** */
void define_monster(int mid);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: debug - itemname - mon-util
 * *********************************************************************** */
void moname(int mcl, bool vis, char descrip, char glog[ ITEMNAME_SIZE ]);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void mons_spell_list(unsigned char sec, int splist[6]);

#if DEBUG_DIAGNOSTICS
// last updated 25sep2001 {dlb}
/* ***********************************************************************
 * called from: describe
 * *********************************************************************** */
const char *mons_spell_name( int spell );
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
bool ms_direct_nasty(int monspell);

// last updated 14jan2001 (gdl)
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
bool ms_requires_tracer(int mons_spell);

bool ms_useful_fleeing_out_of_sight( struct monsters *mon, int monspell );
bool ms_waste_of_time( struct monsters *mon, int monspell );
bool ms_low_hitpoint_cast( struct monsters *mon, int monspell );

bool mons_has_ranged_spell( struct monsters *mon );
bool mons_has_ranged_attack( struct monsters *mon );

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
bool mons_friendly(struct monsters *m);


int mons_has_ench( struct monsters *mon, unsigned int ench, 
                   unsigned int ench2 = ENCH_NONE );

int mons_del_ench( struct monsters *mon, unsigned int ench, 
                   unsigned int ench2 = ENCH_NONE, bool quiet = false );

bool mons_add_ench( struct monsters *mon, unsigned int ench );


bool check_mons_resist_magic( struct monsters *monster, int pow );

#endif
