/*
 *  File:       effects.h
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef EFFECTS_H
#define EFFECTS_H

#include "externs.h"

struct bolt;

class monsters;
struct item_def;


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - beam - decks - fight - religion - spells
 * *********************************************************************** */
void banished(dungeon_feature_type gate_type, const std::string &who = "");


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: spells
 * *********************************************************************** */
bool forget_spell(void);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: fight - it_use2 - it_use3 - items - religion - spells -
 *              spells2 - spells4
 * *********************************************************************** */
bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               bool force = false, const std::string cause = "",
               bool see_source = true);
bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               bool force = false, const char* cause = NULL,
               bool see_source = true);
bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               const monsters* cause, bool force = false);
bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               const item_def &cause, bool removed, bool force = false);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: item_use - spell - spells
 * *********************************************************************** */
void random_uselessness(unsigned char ru, unsigned char sc_read_2);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - decks - item_use - religion
 * *********************************************************************** */
bool acquirement(object_class_type force_class, int agent,
                 bool quiet = false, int *item_index = NULL);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: item_use
 * *********************************************************************** */
bool recharge_wand(const int item_slot = -1);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: mstuff2
 * *********************************************************************** */
void direct_effect(struct bolt &pbolt);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: mstuff2
 * *********************************************************************** */
void mons_direct_effect(struct bolt &pbolt, int i);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void yell(bool force = false);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: item_use - spell
 * *********************************************************************** */
bool holy_word_player(int pow, int caster);
bool holy_word(int pow, int caster, bool silent = false);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - decks - fight - it_use3 - item_use - mstuff2 -
 *              spell
 * *********************************************************************** */
void torment( int caster, int tx, int ty );

bool torment_player(int pow, int caster);
int torment_monsters(int x, int y, int pow, int caster);

bool forget_inventory(bool quiet = false);
bool vitrify_area(int radius);
void update_corpses(double elapsedTime);
void update_level(double elapsedTime);
void handle_time( long time_delta );

#endif
