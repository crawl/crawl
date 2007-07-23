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

    char AC;             // armour class

    char ev;             // evasion

    char speed, speed_inc;        // duh!

    mon_spellbook_type sec;
    corpse_effect_type corpse_thingy;
    zombie_size_type zombie_size;
    shout_type shouts;
    mon_intel_type intel;
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
bool mons_monster_visible( struct monsters *mon, struct monsters *targ );
bool mons_player_visible( struct monsters *mon );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: view
 * *********************************************************************** */
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

bool mons_behaviour_perceptible(const monsters *mons);
bool mons_is_confused(const monsters *m);
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
