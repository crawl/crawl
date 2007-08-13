
/*
 *  File:       describe.h
 *  Summary:    Functions used to print information about various game objects.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <2>      5/21/99        BWR             Changed from is_artifact to is_dumpable_artifact
 *      <1>      4/20/99        JDJ             Added get_item_description and is_artifact.
 */

#ifndef DESCRIBE_H
#define DESCRIBE_H

#include <string>
#include "externs.h"
#include "enum.h"

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: chardump - spells4
 * *********************************************************************** */
bool is_dumpable_artefact( const item_def &item, bool verbose );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: chardump - describe
 * *********************************************************************** */
std::string get_item_description( const item_def &item, bool verbose,
                                  bool dump = false );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - religion
 * *********************************************************************** */
void describe_god( god_type which_god, bool give_title );

void describe_feature_wide(int x, int y);

/* ***********************************************************************
 * called from: item_use - shopping
 * *********************************************************************** */
void describe_item( item_def &item );

/* ***********************************************************************
 * called from: direct
 * *********************************************************************** */
void describe_monsters(monsters &mons);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: item_use
 * *********************************************************************** */
void describe_spell(spell_type spelled);

// last updated 13oct2003 {darshan}
/* ***********************************************************************
 * called from: describe_monsters - describe, kill_ghost - Kills
 * *********************************************************************** */
std::string ghost_description(const monsters &mons, bool concise = false);

const char *trap_name(trap_type trap);
int str_to_trap(const std::string &s);

#endif
