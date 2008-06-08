/*
 *  File:       spells3.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SPELLS3_H
#define SPELLS3_H


struct dist;
struct bolt;

// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spells1 - spells3
 * *********************************************************************** */
bool allow_control_teleport(bool quiet = false);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
int airstrike(int power, dist &beam);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
bool cast_bone_shards(int power, bolt &);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
void cast_poison_ammo(void);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
bool cast_selective_amnesia(bool force);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
int cast_smiting(int power, dist &);
bool remove_sanctuary(bool did_attack = false);
void decrease_sanctuary_radius(void);
bool cast_sanctuary(const int power);
int halo_radius(void);
bool inside_halo(int posx, int posy);

// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
bool project_noise(void);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: item_use - spell
 * *********************************************************************** */
bool detect_curse(bool suppress_msg);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: decks - spell
 * *********************************************************************** */
bool entomb(int powc);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
int portal(void);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
bool recall(char type_recalled);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: item_use - spell
 * *********************************************************************** */
bool remove_curse(bool suppress_msg);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
bool cast_sublimation_of_blood(int pow);

bool cast_summon_horrible_things(int pow, bool god_gift = false);

bool cast_simulacrum(int pow, bool god_gift = false);
bool cast_shadow_creatures(bool god_gift = false);
bool cast_summon_wraiths(int pow, bool god_gift = false);
bool cast_death_channel(int pow, bool god_gift = false);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - beam - decks - fight - item_use - spell
 * *********************************************************************** */
void you_teleport(void);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - decks - effects - fight - misc - spells
 * *********************************************************************** */
void you_teleport_now( bool allow_control, bool new_abyss_area = false );


#endif
